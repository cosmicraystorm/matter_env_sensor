#include "AirMonitorCore.h"
#include "../entities/DelayAwaiter.hpp"

entities::Task<void> usecases::AirMonitorCore::Run() {
    co_await entities::delay_ms(kSensorWarmUpMs);

    while (true) {
        co_await wakeSignal_.Wait();
        if (hasMeasured_ && xTaskGetTickCount() - lastMeasuredTick_ < pdMS_TO_TICKS(kMinMeasurePeriodMs)) {
            continue;   // 自分の送信で起きた分などは無視する
        }

        const auto airCondition = co_await MeasureWithRetry();
        if (airCondition) {
            sensorErrorReported_ = false;
            statusIndicator_.OnSensorRecovered();
            co_await linkage_.Notify(*airCondition);
        } else {
            // 異常表示は送信の成否に関わらず出す(正常に測定できるまで継続)
            statusIndicator_.OnSensorError();
            if (!sensorErrorReported_) {
                // 異常は1回だけ通知し、正常に測定できるまで更新しない
                sensorErrorReported_ = co_await linkage_.NotifySensorError();
            }
        }
    }
}

entities::Task<std::optional<entities::AirCondition>> usecases::AirMonitorCore::MeasureWithRetry() {
    for (int attempt = 0; ; ++attempt) {
        if (hasMeasured_) {
            const TickType_t elapsed = xTaskGetTickCount() - lastMeasuredTick_;
            const TickType_t minInterval = pdMS_TO_TICKS(kSensorMinIntervalMs);
            if (elapsed < minInterval) {
                co_await entities::DelayAwaiter(minInterval - elapsed);
            }
        }

        auto airCondition = co_await sensor_.Measure();
        lastMeasuredTick_ = xTaskGetTickCount();
        hasMeasured_ = true;

        if (airCondition || attempt >= kMeasureRetryCount) {
            co_return airCondition;
        }
    }
}
