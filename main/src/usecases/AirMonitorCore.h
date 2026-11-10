#pragma once

#include <cstdint>
#include <optional>
#include <freertos/FreeRTOS.h>
#include "../entities/CoroTask.hpp"
#include "../entities/AsyncSignal.hpp"
#include "AirConditionSensor.h"
#include "Linkage.h"
#include "StatusIndicator.h"

namespace usecases {

/// @brief AM2302は電源投入後2秒間は測定不可
constexpr std::uint32_t kSensorWarmUpMs = 2000;
/// @brief AM2302の最小測定間隔
constexpr std::uint32_t kSensorMinIntervalMs = 2000;
/// @brief 測定周期の下限。これより短い間隔の起床通知では測定しない。
/// 送信(レポート)自体で SED が Idle→Active に遷移して起床通知が来るため、
/// これが無いと「送信→起床→測定→送信…」と最小測定間隔(2秒)ごとに測定し続けてしまう。
/// ICD の Idle 期間(30秒)より十分短く、測定+送信の所要時間より十分長い値にする
constexpr std::uint32_t kMinMeasurePeriodMs = 20000;
/// @brief 測定失敗時のリトライ回数(単発の読み取りミスでエラー扱いにしないため)
constexpr int kMeasureRetryCount = 1;

/// @brief 起床通知のたびに測定して通知する
class AirMonitorCore {
private:
    AirConditionSensor& sensor_;
    Linkage& linkage_;
    entities::AsyncSignal& wakeSignal_;
    StatusIndicator& statusIndicator_;

    TickType_t lastMeasuredTick_ = 0;
    bool hasMeasured_ = false;
    /// @brief センサ異常を通知済みか(正常に測定できるまで再通知しない)
    bool sensorErrorReported_ = false;

    entities::Task<std::optional<entities::AirCondition>> MeasureWithRetry();

public:
    AirMonitorCore(AirConditionSensor& sensor, Linkage& linkage, entities::AsyncSignal& wakeSignal,
                   StatusIndicator& statusIndicator)
        : sensor_(sensor), linkage_(linkage), wakeSignal_(wakeSignal), statusIndicator_(statusIndicator) {}

    /// @brief 起床通知→測定→通知を繰り返す(終了しない)
    entities::Task<void> Run();
};

}   // namespace usecases
