#include <optional>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <common_macros.h>
#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <platform/ESP32/OpenthreadLauncher.h>
#include <esp_openthread_types.h>

#include "src/entities/AsyncSignal.hpp"
#include "src/entities/MainThreadDispatcher.hpp"
#include "src/usecases/AirMonitorCore.h"
#include "src/usecases/StatusIndicator.h"
#include "src/frameworksanddrivers/PinAssign.h"
#include "src/frameworksanddrivers/XiaoBoard.hpp"
#include "src/frameworksanddrivers/StatusLed.hpp"
#include "src/frameworksanddrivers/Am2302Sensor.h"
#include "src/frameworksanddrivers/MatterAirLinkage.h"
#if CONFIG_ENABLE_ICD_SERVER
#include "src/frameworksanddrivers/MatterIcdObserver.hpp"
#else
#include "src/entities/DelayAwaiter.hpp"
#endif

static const char* TAG = "app_main";

using namespace esp_matter;
using chip::DeviceLayer::ChipDeviceEvent;
namespace DeviceEventType = chip::DeviceLayer::DeviceEventType;

// ---- 組み立て ----
static frameworksanddrivers::StatusLed statusLed(frameworksanddrivers::USER_LED_PIN, frameworksanddrivers::USER_LED_ON_LEVEL);
static usecases::StatusIndicator statusIndicator(statusLed);
static entities::AsyncSignal wakeSignal;
static frameworksanddrivers::Am2302Sensor airSensor(frameworksanddrivers::AM2302_DATA_PIN);
static frameworksanddrivers::MatterAirLinkage matterLinkage;
static usecases::AirMonitorCore airMonitor(airSensor, matterLinkage, wakeSignal, statusIndicator);
#if CONFIG_ENABLE_ICD_SERVER
static frameworksanddrivers::MatterIcdObserver icdObserver(statusIndicator, wakeSignal);
#endif

// 実行中コルーチンの所有者(破棄されるとフレームも消えるので保持し続ける)
static std::optional<entities::Task<void>> airMonitorTask;

#if !CONFIG_ENABLE_ICD_SERVER
/// @brief ICD 無効ビルド用: SED 復帰イベントの代わりに 30 秒ごとに測定を起動する
static constexpr std::uint32_t kFallbackWakeIntervalMs = 30000;
static std::optional<entities::Task<void>> fallbackWakeTask;

static entities::Task<void> FallbackWakeLoop() {
    while (true) {
        co_await entities::delay_ms(kFallbackWakeIntervalMs);
        statusIndicator.OnWake();
        wakeSignal.Set();
    }
}
#endif

static bool HasFabric() {
    return chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
}

/// @brief 通信確立(コミッショニング済みで Thread ネットワークに参加済み)
static void OnNetworkEstablished() {
    ESP_LOGI(TAG, "Network established");
    statusIndicator.OnNetworkEstablished();
    wakeSignal.Set();   // 確立直後に1回測定する
}

static void OpenCommissioningWindowIfNecessary() {
    VerifyOrReturn(!HasFabric());
    chip::CommissioningWindowManager& commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
    VerifyOrReturn(!commissionMgr.IsCommissioningWindowOpen());
    // Thread の資格情報は残っているので DNS-SD のみで再コミッショニングを待つ
    CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(chip::System::Clock::Seconds16(300),
                                                                chip::CommissioningWindowAdvertisement::kDnssdOnly);
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
    }
}

// CHIP スレッドで呼ばれる
static void AppEventCallback(const ChipDeviceEvent* event, intptr_t arg) {
    switch (event->Type) {
    case DeviceEventType::kServerReady:
#if CONFIG_ENABLE_ICD_SERVER
        chip::Server::GetInstance().GetICDManager().RegisterObserver(&icdObserver);
#endif
        if (HasFabric() && chip::DeviceLayer::ConnectivityMgr().IsThreadAttached()) {
            OnNetworkEstablished();
        }
        break;

    case DeviceEventType::kThreadConnectivityChange:
        // 起動時: コミッショニング済みなら Thread 参加で確立
        if (event->ThreadConnectivityChange.Result == chip::DeviceLayer::kConnectivity_Established && HasFabric()) {
            OnNetworkEstablished();
        }
        break;

    case DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        OnNetworkEstablished();
        break;

    case DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Fabric removed");
        if (!HasFabric()) {
            statusIndicator.OnNetworkLost();
        }
        OpenCommissioningWindowIfNecessary();
        break;

    default:
        break;
    }
}

static esp_err_t AppIdentificationCallback(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                           uint8_t effect_variant, void* priv_data) {
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}

static esp_err_t AppAttributeUpdateCallback(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                            uint32_t attribute_id, esp_matter_attr_val_t* val, void* priv_data) {
    // センサのみで書き込み可能な属性は扱わない
    return ESP_OK;
}

extern "C" void app_main() {
    frameworksanddrivers::InitXiaoRfSwitch();
    statusLed.Begin();
    statusIndicator.OnBoot();

    nvs_flash_init();

    esp_err_t err = ESP_OK;
#if CONFIG_PM_ENABLE
    // 自動 Light Sleep を有効化 (USBデバッグビルドでは無効)
    esp_pm_config_t pmConfig = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .light_sleep_enable = true,
    };
    err = esp_pm_configure(&pmConfig);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to configure PM, err:%d", err));
#endif

    node::config_t nodeConfig;
    node_t* node = node::create(&nodeConfig, AppAttributeUpdateCallback, AppIdentificationCallback);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

#if CONFIG_ENABLE_ICD_SERVER
    // Aqara Home (M100/M200) は ICD Management クラスタ(0x0046)を持つデバイスの
    // コミッショニングを途中で中断する。ICD の動作(SED・Active/Idle 周期)は残したまま、
    // クラスタだけをデータモデルから外してコントローラーからは見えないようにする
    if (cluster_t* icdCluster = cluster::get(static_cast<uint16_t>(0), chip::app::Clusters::IcdManagement::Id)) {
        err = cluster::destroy(icdCluster);
        ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to remove ICD Management cluster, err:%d", err));
    }
#endif
    ABORT_APP_ON_FAILURE(matterLinkage.CreateEndpoints(node), ESP_LOGE(TAG, "Failed to create sensor endpoints"));

    esp_openthread_platform_config_t threadConfig = {
        .radio_config = { .radio_mode = RADIO_MODE_NATIVE },
        .host_config = { .host_connection_mode = HOST_CONNECTION_MODE_NONE },
        .port_config = { .storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10 },
    };
    set_openthread_platform_config(&threadConfig);

    err = esp_matter::start(AppEventCallback);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    // ホットタスクなので最初の co_await まで即時実行され、以降はディスパッチャ経由で再開される
    airMonitorTask.emplace(airMonitor.Run());
#if !CONFIG_ENABLE_ICD_SERVER
    fallbackWakeTask.emplace(FallbackWakeLoop());
#endif

    // メインタスクはコルーチンの再開待ち。キューで Blocked の間は Light Sleep に入れる
    while (true) {
        entities::MainThreadDispatcher::instance().WaitAndExecute();
    }
}
