#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include "Indicator.h"

namespace usecases {

/// @brief SED 復帰時の点灯時間(起きたことが分かれば十分なので短く)
constexpr std::uint32_t kWakePulseMs = 50;
/// @brief センサ異常時の点滅: 50ms点灯 → 50ms消灯 → 50ms点灯 → 消灯
constexpr std::array<std::uint32_t, 3> kSensorErrorBlinkMs = {50, 50, 50};
/// @brief センサ異常時の点滅周期
constexpr std::uint32_t kSensorErrorBlinkPeriodMs = 1000;

/// @brief LED表示仕様(上ほど優先)
/// 1. 通信未確立(起動直後・全ファブリック削除後): 常時点灯
/// 2. センサ異常: 1秒ごとに2回短く点滅(SED 復帰の点滅はしない)
/// 3. 通常: 消灯。SED 復帰(Active)で短く1回点灯
///
/// CHIP スレッド(ネットワーク・SED イベント)とメインスレッド(測定結果)の両方から呼ばれるので排他する
class StatusIndicator {
private:
    Indicator& indicator_;
    std::mutex mutex_;
    bool established_ = false;
    bool sensorError_ = false;

    /// @brief 状態に応じた常時表示にする(mutex_ 保持中に呼ぶ)
    void Apply() {
        if (!established_) {
            indicator_.TurnOn();
        } else if (sensorError_) {
            indicator_.RepeatPattern(kSensorErrorBlinkMs, kSensorErrorBlinkPeriodMs);
        } else {
            indicator_.TurnOff();
        }
    }

public:
    explicit StatusIndicator(Indicator& indicator) : indicator_(indicator) {}

    void OnBoot() {
        std::lock_guard<std::mutex> lock(mutex_);
        established_ = false;
        Apply();
    }

    void OnNetworkEstablished() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!established_) {
            established_ = true;
            Apply();
        }
    }

    /// @brief 全ファブリックから削除され再コミッショニング待ちになった
    void OnNetworkLost() {
        std::lock_guard<std::mutex> lock(mutex_);
        established_ = false;
        Apply();
    }

    /// @brief リトライしても測定できなかった
    void OnSensorError() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!sensorError_) {    // 点滅の位相を乱さないよう、状態が変わった時だけ切り替える
            sensorError_ = true;
            Apply();
        }
    }

    void OnSensorRecovered() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sensorError_) {
            sensorError_ = false;
            Apply();
        }
    }

    void OnWake() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (established_ && !sensorError_) {
            indicator_.Pulse(kWakePulseMs);
        }
    }

    void OnSleep() {
        // 点滅は自動で消えるので何もしない
    }
};

}   // namespace usecases
