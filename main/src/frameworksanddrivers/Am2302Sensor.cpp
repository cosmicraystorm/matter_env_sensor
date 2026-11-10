#include "Am2302Sensor.h"

#include <esp_log.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>

static const char* TAG = "am2302";

/// @brief ホストからの開始信号(Low)の長さ。データシート: 0.8～20ms
static constexpr std::uint32_t kStartSignalUs = 1100;
/// @brief 1パルスあたりのタイムアウト。正常時は最長でも80us程度
static constexpr std::uint32_t kPulseTimeoutUs = 200;
static constexpr std::uint32_t kTimeout = UINT32_MAX;
static constexpr std::size_t kDataBits = 40;

void frameworksanddrivers::Am2302Sensor::ConfigurePin() {
    // Light Sleep 復帰後はGPIO設定が初期化されている可能性があるので測定ごとに設定する。
    // オープンドレイン+内部プルアップ。HIGH出力=バス解放なので、
    // 以降は方向を切り替えずに set_level/get_level だけで送受信できる
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << pin_,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&config);
    gpio_set_level(pin_, 1);
}

entities::Task<std::optional<entities::AirCondition>> frameworksanddrivers::Am2302Sensor::Measure() {
    ConfigurePin();

    ReadFailure failure{};
    const auto raw = ReadRaw(failure);
    if (!raw) {
        LogFailure(failure);
        co_return std::nullopt;
    }

    const auto airCondition = interfaceadapters::DecodeAm2302(*raw);
    if (!airCondition) {
        LogFailure({Am2302Fault::ChecksumError, -1});
    }
    co_return airCondition;
}

std::uint32_t frameworksanddrivers::Am2302Sensor::ExpectPulse(int level) const {
    const std::int64_t start = esp_timer_get_time();
    while (gpio_get_level(pin_) == level) {
        if (esp_timer_get_time() - start > kPulseTimeoutUs) {
            return kTimeout;
        }
    }
    return static_cast<std::uint32_t>(esp_timer_get_time() - start);
}

std::optional<interfaceadapters::Am2302RawData> frameworksanddrivers::Am2302Sensor::ReadRaw(ReadFailure& failure) {
    // 解放中のバスはプルアップで High のはず。Low なら開始信号を送っても意味がない
    if (gpio_get_level(pin_) == 0) {
        failure = {Am2302Fault::BusStuckLow, -1};
        return std::nullopt;
    }

    // 開始信号
    gpio_set_level(pin_, 0);
    esp_rom_delay_us(kStartSignalUs);

    std::uint32_t lowUs[kDataBits];
    std::uint32_t highUs[kDataBits];
    bool ok = true;

    // 受信中(約5ms)は割り込みで時間計測が乱れないようにする。
    // クリティカルセクション内ではログを出さず、失敗内容は記録だけして後で出す
    portENTER_CRITICAL(&mux_);
    gpio_set_level(pin_, 1);    // バス解放
    // センサ応答: 解放後20-40us High → 80us Low → 80us High
    if (ExpectPulse(1) == kTimeout) {
        ok = false;
        failure = {Am2302Fault::NoResponse, -1};
    } else if (ExpectPulse(0) == kTimeout) {
        ok = false;
        failure = {Am2302Fault::ResponseLowTimeout, -1};
    } else if (ExpectPulse(1) == kTimeout) {
        ok = false;
        failure = {Am2302Fault::ResponseHighTimeout, -1};
    }
    // 各ビット: 50us Low の後、High が 26-28us なら0、70us なら1
    for (std::size_t i = 0; ok && i < kDataBits; ++i) {
        lowUs[i] = ExpectPulse(0);
        highUs[i] = ExpectPulse(1);
        if (lowUs[i] == kTimeout || highUs[i] == kTimeout) {
            ok = false;
            failure = {Am2302Fault::DataTimeout, static_cast<int>(i)};
        }
    }
    portEXIT_CRITICAL(&mux_);

    if (!ok) {
        return std::nullopt;
    }

    interfaceadapters::Am2302RawData raw{};
    for (std::size_t i = 0; i < kDataBits; ++i) {
        raw[i / 8] <<= 1;
        if (highUs[i] > lowUs[i]) {
            raw[i / 8] |= 1;
        }
    }
    return raw;
}

void frameworksanddrivers::Am2302Sensor::LogFailure(const ReadFailure& failure) const {
    switch (failure.fault) {
    case Am2302Fault::BusStuckLow:
        ESP_LOGW(TAG, "bus stuck low (check DATA short to GND / pull-up)");
        break;
    case Am2302Fault::NoResponse:
        ESP_LOGW(TAG, "no response (check DATA pin, sensor VCC/GND)");
        break;
    case Am2302Fault::ResponseLowTimeout:
        ESP_LOGW(TAG, "response low timeout");
        break;
    case Am2302Fault::ResponseHighTimeout:
        ESP_LOGW(TAG, "response high timeout");
        break;
    case Am2302Fault::DataTimeout:
        ESP_LOGW(TAG, "data timeout at bit %d (check contact / noise)", failure.bitIndex);
        break;
    case Am2302Fault::ChecksumError:
        ESP_LOGW(TAG, "checksum error (noise)");
        break;
    }
}
