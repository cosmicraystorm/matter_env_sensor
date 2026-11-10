#include "MatterAirLinkage.h"

#include <algorithm>
#include <cmath>
#include <coroutine>
#include <esp_log.h>
#include <platform/CHIPDeviceLayer.h>
#include "../entities/MainThreadDispatcher.hpp"

using namespace esp_matter;
using namespace chip::app::Clusters;

static const char* TAG = "matter_air";

bool frameworksanddrivers::MatterAirLinkage::CreateEndpoints(node_t* node) {
    endpoint::temperature_sensor::config_t temperatureConfig;
    endpoint_t* temperatureEndpoint = endpoint::temperature_sensor::create(node, &temperatureConfig, ENDPOINT_FLAG_NONE, nullptr);

    endpoint::humidity_sensor::config_t humidityConfig;
    endpoint_t* humidityEndpoint = endpoint::humidity_sensor::create(node, &humidityConfig, ENDPOINT_FLAG_NONE, nullptr);

    if (temperatureEndpoint == nullptr || humidityEndpoint == nullptr) {
        return false;
    }
    temperatureEndpointId_ = endpoint::get_id(temperatureEndpoint);
    humidityEndpointId_ = endpoint::get_id(humidityEndpoint);
    return true;
}

entities::Task<bool> frameworksanddrivers::MatterAirLinkage::Notify(const entities::AirCondition& airCondition) {
    ESP_LOGI(TAG, "Temperature: %.1fC, Humidity: %.1f%%", airCondition.temperatureCelsius, airCondition.humidityPercent);

    // Matter の単位はどちらも 0.01 (温度: 0.01℃, 湿度: 0.01%)
    const auto temperature = static_cast<std::int16_t>(std::lround(airCondition.temperatureCelsius * 100.0));
    const auto humidity = static_cast<std::uint16_t>(std::clamp(std::lround(airCondition.humidityPercent * 100.0), 0L, 10000L));
    co_return co_await Update(nullable<std::int16_t>(temperature), nullable<std::uint16_t>(humidity));
}

entities::Task<bool> frameworksanddrivers::MatterAirLinkage::NotifySensorError() {
    ESP_LOGW(TAG, "Sensor error: report null");
    co_return co_await Update(nullable<std::int16_t>(), nullable<std::uint16_t>());
}

entities::Task<bool> frameworksanddrivers::MatterAirLinkage::Update(nullable<std::int16_t> temperature, nullable<std::uint16_t> humidity) {
    struct ChipThreadAwaiter {
        MatterAirLinkage& self_;
        nullable<std::int16_t> temperature_;
        nullable<std::uint16_t> humidity_;
        std::coroutine_handle<> handle_ = nullptr;
        bool result_ = false;

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            handle_ = h;
            // 属性は CHIP スレッドからのみ更新できる
            const CHIP_ERROR err = chip::DeviceLayer::SystemLayer().ScheduleLambda([this]() {
                esp_matter_attr_val_t temperatureVal = esp_matter_nullable_int16(temperature_);
                esp_matter_attr_val_t humidityVal = esp_matter_nullable_uint16(humidity_);
                const esp_err_t temperatureErr = attribute::update(self_.temperatureEndpointId_, TemperatureMeasurement::Id,
                    TemperatureMeasurement::Attributes::MeasuredValue::Id, &temperatureVal);
                const esp_err_t humidityErr = attribute::update(self_.humidityEndpointId_, RelativeHumidityMeasurement::Id,
                    RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &humidityVal);
                result_ = (temperatureErr == ESP_OK) && (humidityErr == ESP_OK);
                entities::MainThreadDispatcher::instance().Post(handle_);
            });
            if (err != CHIP_NO_ERROR) {
                result_ = false;
                entities::MainThreadDispatcher::instance().Post(h);
            }
        }

        bool await_resume() const noexcept { return result_; }
    };

    const bool result = co_await ChipThreadAwaiter{*this, temperature, humidity};
    if (!result) {
        ESP_LOGE(TAG, "Failed to update attributes");
    }
    co_return result;
}
