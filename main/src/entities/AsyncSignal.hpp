#pragma once
#include <coroutine>
#include <mutex>
#include "MainThreadDispatcher.hpp"

namespace entities {

/// @brief 別スレッドからの通知をコルーチンで待つための自動リセット型シグナル
/// 待機中のコルーチンがいなければ通知を1件だけ保持し、次の Wait() で即座に返る
class AsyncSignal {
private:
    std::mutex mutex_;
    bool pending_ = false;
    std::coroutine_handle<> waiting_ = nullptr;

public:
    /// @brief 通知する(どのスレッドからでも可)。待機中のコルーチンはメインスレッドで再開される
    void Set() {
        std::coroutine_handle<> resumeTarget = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (waiting_) {
                resumeTarget = waiting_;
                waiting_ = nullptr;
            } else {
                pending_ = true;
            }
        }
        if (resumeTarget) {
            MainThreadDispatcher::instance().Post(resumeTarget);
        }
    }

    /// @brief 通知を待つ(待機できるコルーチンは同時に1つだけ)
    auto Wait() {
        struct Awaiter {
            AsyncSignal& self_;

            bool await_ready() const noexcept { return false; }

            // 判定と登録を同じロック内で行い、Set() との競合で通知を取りこぼさないようにする
            bool await_suspend(std::coroutine_handle<> h) {
                std::lock_guard<std::mutex> lock(self_.mutex_);
                if (self_.pending_) {
                    self_.pending_ = false;
                    return false;   // 中断せずにそのまま続行
                }
                self_.waiting_ = h;
                return true;
            }

            void await_resume() const noexcept {}
        };
        return Awaiter{*this};
    }
};

}   // namespace entities
