#pragma once

#include <driver/gpio.h>

/// XIAO ESP32C6 のピン割り当て
namespace frameworksanddrivers {
    /// @brief AM2302 データ線 (D0)
    constexpr gpio_num_t AM2302_DATA_PIN = GPIO_NUM_0;

    /// @brief ユーザーLED(黄)。LOWで点灯
    constexpr gpio_num_t USER_LED_PIN = GPIO_NUM_15;
    constexpr int USER_LED_ON_LEVEL = 0;

    /// @brief RFスイッチ電源。LOWで有効
    constexpr gpio_num_t RF_SWITCH_ENABLE_PIN = GPIO_NUM_3;
    /// @brief アンテナ選択。LOW:内蔵セラミックアンテナ / HIGH:外部アンテナ
    constexpr gpio_num_t RF_ANTENNA_SELECT_PIN = GPIO_NUM_14;
}
