#pragma once
#include <coroutine>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include "MainThreadDispatcher.hpp"

namespace entities {

struct DelayAwaiter {
    TickType_t ticks_to_wait;
    TimerHandle_t timer_handle = nullptr;
    std::coroutine_handle<> awaiting_coroutine;

    DelayAwaiter(TickType_t ticks) : ticks_to_wait(ticks) {}

    bool await_ready() const noexcept { return ticks_to_wait == 0; }

    void await_suspend(std::coroutine_handle<> h) {
        awaiting_coroutine = h;

        // タイマー作成
        timer_handle = xTimerCreate(
            "co_delay",
            ticks_to_wait,
            pdFALSE,
            this, // 💡 ここで自分自身(this)をタイマーのIDとして登録します
            [](TimerHandle_t xTimer) {
                // 💡 【修正点】pvTimerGetTimerID に修正（pを追加）
                // これにより、xTimerCreateの第4引数に渡した「this（DelayAwaiterのポインタ）」が返ってきます
                auto* awater = static_cast<DelayAwaiter*>(pvTimerGetTimerID(xTimer));
                
                // タイマーは使い捨てなのでここで削除
                xTimerDelete(xTimer, 0);

                if (awater && awater->awaiting_coroutine) {
                    // メインスレッドのディッパッチャーにコルーチンを投げる
                    MainThreadDispatcher::instance().Post(awater->awaiting_coroutine);
                }
            }
        );

        if (timer_handle != nullptr) {
            xTimerStart(timer_handle, 0);
        } else {
            // 万が一タイマー作成に失敗した場合は、安全のために即座にメインへPost
            MainThreadDispatcher::instance().Post(h);
        }
    }

    void await_resume() noexcept {}
};

// 💡 呼び出し用のヘルパー関数
inline DelayAwaiter delay_ms(uint64_t ms) {
    return DelayAwaiter(pdMS_TO_TICKS(ms));
}

} // namespace entities