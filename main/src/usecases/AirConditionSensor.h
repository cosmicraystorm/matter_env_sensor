#pragma once

#include <optional>
#include "../entities/CoroTask.hpp"
#include "../entities/AirCondition.h"

namespace usecases {

/// @brief 温湿度センサの抽象
class AirConditionSensor {
public:
    virtual ~AirConditionSensor() = default;

    /// @brief 温湿度を1回測定する
    /// @return 測定値。失敗時は std::nullopt
    virtual entities::Task<std::optional<entities::AirCondition>> Measure() = 0;
};

}   // namespace usecases
