#pragma once

#include <driver/gpio.h>

namespace frameworksanddrivers {

/// @brief Light Sleep 中も出力レベルを保持する出力ピン
/// 周辺電源を落とす Light Sleep ではGPIO設定が保たれないため、レベル変更のたびにホールドを掛け直す
class HeldOutputPin {
private:
    const gpio_num_t pin_;

public:
    explicit HeldOutputPin(gpio_num_t pin) : pin_(pin) {}

    void Begin(int initialLevel) {
        gpio_hold_dis(pin_);
        gpio_reset_pin(pin_);
        gpio_set_direction(pin_, GPIO_MODE_OUTPUT);
        Set(initialLevel);
    }

    void Set(int level) {
        gpio_hold_dis(pin_);
        gpio_set_level(pin_, level);
        gpio_hold_en(pin_);
    }
};

}   // namespace frameworksanddrivers
