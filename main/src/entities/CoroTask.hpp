#pragma once
#include <coroutine>
#include <exception>
#include <utility>

namespace entities {

// --- 値を返すタスク ---
template <typename T>
struct Task {
    struct promise_type {
        T value;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

        Task get_return_object() { return Task(std::coroutine_handle<promise_type>::from_promise(*this)); }
        
        // 💡 1. 即時実行（ホットタスク）にするため、suspend_never に変更
        std::suspend_never initial_suspend() { return {}; }
        
        // 💡 2. 【バグ修正】final_awaiterの中で外側のhandleを探さないよう、正しい再開ロジックに修正
        struct final_awaiter {
            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                if (h.promise().continuation) return h.promise().continuation;
                return std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };
        final_awaiter final_suspend() noexcept { return {}; }
        
        void unhandled_exception() { exception = std::current_exception(); }
        void return_value(T val) { value = std::move(val); }
    };

    // 外側の Task<T> のメンバ
    std::coroutine_handle<promise_type> handle;
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task(Task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    ~Task() { if (handle) handle.destroy(); }

    bool await_ready() const noexcept { return !handle || handle.done(); }
    
    // 💡 3. 【配置修正】外側のメンバ関数として定義し、戻り値を void に変更
    void await_suspend(std::coroutine_handle<> awaiting) noexcept {
        handle.promise().continuation = awaiting;
    }
    
    T await_resume() {
        if (handle.promise().exception) std::rethrow_exception(handle.promise().exception);
        return std::move(handle.promise().value);
    }
};

// --- 値を返さないタスク (void) ---
template <>
struct Task<void> {
    struct promise_type {
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

        Task get_return_object() { return Task(std::coroutine_handle<promise_type>::from_promise(*this)); }
        
        // 💡 1. 即時実行（ホットタスク）にするため、suspend_never に変更
        std::suspend_never initial_suspend() { return {}; }
        
        // 💡 2. final_suspend の中身はこのままで固定（触らない）
        struct final_awaiter {
            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                if (h.promise().continuation) return h.promise().continuation;
                return std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };
        final_awaiter final_suspend() noexcept { return {}; }
        
        void unhandled_exception() { exception = std::current_exception(); }
        void return_void() {}
    };

    // ここから外側の Task<void> のメンバ
    std::coroutine_handle<promise_type> handle;
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task(Task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    ~Task() { if (handle) handle.destroy(); }

    bool await_ready() const noexcept { return !handle || handle.done(); }

    // 💡 3. 【ここに配置！】外側のメンバ関数として await_suspend を定義します。
    // 戻り値は void にし、内部で handle を安全に参照します。
    void await_suspend(std::coroutine_handle<> awaiting) noexcept {
        handle.promise().continuation = awaiting;
    }
    
    void await_resume() { 
        if (handle.promise().exception) std::rethrow_exception(handle.promise().exception); 
    }
};
} // namespace entities