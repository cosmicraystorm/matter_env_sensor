#pragma once

#include "../entities/CoroTask.hpp"
#include "../entities/AirCondition.h"

namespace usecases {

/// @brief 測定値の外部通知の抽象
class Linkage {
public:
    virtual ~Linkage() = default;

    /// @brief 測定値を通知する
    /// @return true: 通知成功, false: 通知失敗
    virtual entities::Task<bool> Notify(const entities::AirCondition& airCondition) = 0;

    /// @brief センサ異常(測定値不明)を通知する
    /// @return true: 通知成功, false: 通知失敗
    virtual entities::Task<bool> NotifySensorError() = 0;
};

}   // namespace usecases
