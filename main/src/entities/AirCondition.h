#pragma once

namespace entities {

/// @brief 1回分の空気状態の測定値
struct AirCondition {
    /// @brief 温度[℃]
    double temperatureCelsius;
    /// @brief 相対湿度[%]
    double humidityPercent;
};

}   // namespace entities
