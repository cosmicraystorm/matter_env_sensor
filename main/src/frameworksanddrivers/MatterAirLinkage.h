#pragma once

#include <cstdint>
#include <esp_matter.h>
#include "../usecases/Linkage.h"

namespace frameworksanddrivers {

/// @brief 測定値を Matter の温度センサ/湿度センサ エンドポイントとして公開する
class MatterAirLinkage : public usecases::Linkage {
private:
    std::uint16_t temperatureEndpointId_ = 0;
    std::uint16_t humidityEndpointId_ = 0;

    /// @brief 属性更新を CHIP スレッドで実行し、完了後にメインスレッドで再開する
    entities::Task<bool> Update(nullable<std::int16_t> temperature, nullable<std::uint16_t> humidity);

public:
    /// @brief 温度センサ・湿度センサのエンドポイントを作成する(esp_matter::start() 前に呼ぶ)
    bool CreateEndpoints(esp_matter::node_t* node);

    entities::Task<bool> Notify(const entities::AirCondition& airCondition) override;
    /// @brief 温度・湿度とも null (測定値不明) にする
    entities::Task<bool> NotifySensorError() override;
};

}   // namespace frameworksanddrivers
