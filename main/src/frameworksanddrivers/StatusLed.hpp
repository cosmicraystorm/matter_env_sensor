#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <esp_timer.h>
#include "../usecases/Indicator.h"
#include "HeldOutputPin.hpp"

namespace frameworksanddrivers {

/// @brief 単色LED
/// 呼び出し元(CHIPスレッド・メインスレッド)とタイマー(esp_timerタスク)の両方から操作されるので排他する。
/// Pulse/RepeatPattern は1本のワンショットタイマーで次の切り替えを予約する(Light Sleep 中もタイマーで起床する)
class StatusLed : public usecases::Indicator {
private:
    /// @brief パターンの最大ステップ数(周期の残りの消灯を含む)
    static constexpr std::size_t kMaxSteps = 8;

    enum class Mode { Steady, Pulse, Pattern };

    HeldOutputPin pin_;
    const int onLevel_;
    std::mutex mutex_;
    esp_timer_handle_t timer_ = nullptr;

    Mode mode_ = Mode::Steady;
    std::array<std::uint32_t, kMaxSteps> steps_{};
    std::size_t stepCount_ = 0;
    std::size_t stepIndex_ = 0;

    // ---- 以下 mutex_ 保持中に呼ぶ ----
    void SetLit(bool lit) {
        pin_.Set(lit ? onLevel_ : !onLevel_);
    }

    void StopTimer() {
        if (timer_) {
            esp_timer_stop(timer_);     // 動いていなければエラーが返るだけなので無視
        }
    }

    void StartTimer(std::uint32_t ms) {
        if (timer_) {
            esp_timer_start_once(timer_, static_cast<std::uint64_t>(ms) * 1000ULL);
        }
    }

    /// @brief パターンの stepIndex_ 番目を表示して次を予約する。偶数ステップが点灯
    void ShowStep() {
        SetLit(stepIndex_ % 2 == 0);
        StartTimer(steps_[stepIndex_]);
    }

    void SetSteady(bool lit) {
        StopTimer();
        mode_ = Mode::Steady;
        SetLit(lit);
    }
    // ----

    void OnTimer() {
        std::lock_guard<std::mutex> lock(mutex_);
        // 発火直後に別スレッドが待ち受けを貼り直していたら、この発火は古いので無視する
        if (timer_ && esp_timer_is_active(timer_)) {
            return;
        }
        switch (mode_) {
        case Mode::Pulse:
            mode_ = Mode::Steady;
            SetLit(false);
            break;
        case Mode::Pattern:
            stepIndex_ = (stepIndex_ + 1) % stepCount_;
            ShowStep();
            break;
        case Mode::Steady:
            break;
        }
    }

public:
    StatusLed(gpio_num_t pin, int onLevel) : pin_(pin), onLevel_(onLevel) {}

    void Begin() {
        std::lock_guard<std::mutex> lock(mutex_);
        pin_.Begin(!onLevel_);
        const esp_timer_create_args_t args = {
            .callback = [](void* arg) { static_cast<StatusLed*>(arg)->OnTimer(); },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "status_led",
            .skip_unhandled_events = false,
        };
        esp_timer_create(&args, &timer_);
    }

    void TurnOn() override {
        std::lock_guard<std::mutex> lock(mutex_);
        SetSteady(true);
    }

    void TurnOff() override {
        std::lock_guard<std::mutex> lock(mutex_);
        SetSteady(false);
    }

    void Pulse(std::uint32_t durationMs) override {
        std::lock_guard<std::mutex> lock(mutex_);
        StopTimer();
        mode_ = Mode::Pulse;
        SetLit(true);
        StartTimer(durationMs);
    }

    void RepeatPattern(std::span<const std::uint32_t> stepsMs, std::uint32_t periodMs) override {
        std::lock_guard<std::mutex> lock(mutex_);
        StopTimer();

        // 周期の残りを消灯ステップとして足す。点灯/消灯は添字の偶奇で決まるので、
        // 指定ステップ数が偶数(最後が消灯)なら最後の消灯に残りを足し込む
        std::size_t count = 0;
        std::uint32_t total = 0;
        for (const auto ms : stepsMs) {
            if (count >= kMaxSteps - 1) {
                break;
            }
            steps_[count++] = ms;
            total += ms;
        }
        if (count == 0) {
            SetSteady(false);
            return;
        }
        const std::uint32_t rest = periodMs > total ? periodMs - total : 0;
        if (count % 2 == 0) {
            steps_[count - 1] += rest;
        } else if (rest > 0) {
            steps_[count++] = rest;
        }
        stepCount_ = count;
        stepIndex_ = 0;
        mode_ = Mode::Pattern;
        ShowStep();
    }
};

}   // namespace frameworksanddrivers
