#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <coroutine>

namespace entities {

class MainThreadDispatcher {
private:
    QueueHandle_t queue_;
    
    MainThreadDispatcher() {
        // コルーチンハンドル（ポインタサイズ）を保持するキューを作成（キャパシティは32）
        queue_ = xQueueCreate(32, sizeof(std::coroutine_handle<>));
    }

public:
    // シングルトンインスタンス
    static MainThreadDispatcher& instance() {
        static MainThreadDispatcher inst;
        return inst;
    }

    // 💡 C# の SynchronizationContext.Post に相当
    // どのスレッドから呼ばれても、メインスレッドのキューに安全にタスクを追加する
    bool Post(std::coroutine_handle<> h) {
        if (!queue_ || !h) return false;
        
        // ISR（割り込み）から呼ばれる可能性もある場合は xQueueSendFromISR を使いますが、
        // 今回はタイマータスク（通常タスク）からなので通常の Send でOK
        return xQueueSend(queue_, &h, 0) == pdPASS;
    }

    // 💡 メインスレッド（Arduinoのloopなど）から毎フレーム呼び出す
    // キューに溜まった「再開待ちコルーチン」をメインスレッド側で実行する
    void Poll() {
        if (!queue_) return;

        std::coroutine_handle<> h;
        // キューが空になるまで、溜まっているコルーチンをすべてノンブロッキングで処理
        while (xQueueReceive(queue_, &h, 0) == pdPASS) {
            if (h && !h.done()) {
                // ★ ここで resume することで、これ以降の処理が
                // 「メインスレッドのコンテキスト」で実行されるようになります！
                h.resume(); 
            }
        }
    }

    /// @brief メインスレッドで待機して、キューにタスクが来るのを待つ（ESP32が自動でライトスリープに入る）
    void WaitAndExecute() {
        if (!queue_) return;

        std::coroutine_handle<> h;
        
        // ★ portMAX_DELAY を指定することで、キューにデータが入るまで
        // このメインタスクは FreeRTOS によって完全に「Blocked」状態になります。
        // これにより、他にタスクがなければESP32は即座に自動ライトスリープに入れます。
        if (xQueueReceive(queue_, &h, portMAX_DELAY) == pdPASS) {
            if (h && !h.done()) {
                h.resume(); 
            }
        }
        
        // 連続してタスクが届いている場合は、残りをすべて消化する
        while (xQueueReceive(queue_, &h, 0) == pdPASS) {
            if (h && !h.done()) {
                h.resume();
            }
        }
    }

    // 💡 Deep Sleep用：指定時間（timeout_ticks）だけキューを待つ
    void PollWithTimeout(TickType_t timeout_ticks) {
        if (!queue_) return;

        std::coroutine_handle<> h;
        
        // 指定した時間だけブロックして待つ（この待機中も自動ライトスリープが効きます）
        if (xQueueReceive(queue_, &h, timeout_ticks) == pdPASS) {
            if (h && !h.done()) {
                h.resume(); 
            }
        }
        
        // 連続してタスクが届いている場合は、残りをすべて一気に消化
        while (xQueueReceive(queue_, &h, 0) == pdPASS) {
            if (h && !h.done()) {
                h.resume();
            }
        }
    }
};

} // namespace entities