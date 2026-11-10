#pragma once

#include <cstdint>
#include <optional>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include "../usecases/AirConditionSensor.h"
#include "../interfaceadapters/Am2302Convert.h"

namespace frameworksanddrivers {

/// @brief AM2302 読み取り失敗の分類(配線・故障の切り分け用にログへ出す)
enum class Am2302Fault {
    /// @brief 開始前からバスが Low。DATA の GND ショート、プルアップ欠落、センサが Low を保持
    BusStuckLow,
    /// @brief 開始信号に応答しない。DATA の接続先違い・断線、センサの電源(VCC/GND)未接続
    NoResponse,
    /// @brief 応答の Low が終わらない
    ResponseLowTimeout,
    /// @brief 応答の High が終わらない
    ResponseHighTimeout,
    /// @brief データ受信の途中で途切れた(ノイズ・接触不良・配線が長い)
    DataTimeout,
    /// @brief 受信はできたがチェックサム不一致(ノイズ)
    ChecksumError,
};

/// @brief AM2302(DHT22) 1-wire温湿度センサ
class Am2302Sensor : public usecases::AirConditionSensor {
private:
    const gpio_num_t pin_;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

    struct ReadFailure {
        Am2302Fault fault;
        /// @brief DataTimeout のとき途切れたビット位置(0～39)
        int bitIndex;
    };

    void ConfigurePin();
    /// @return 受信データ。失敗時は ReadFailure を設定して std::nullopt
    std::optional<interfaceadapters::Am2302RawData> ReadRaw(ReadFailure& failure);
    std::uint32_t ExpectPulse(int level) const;
    void LogFailure(const ReadFailure& failure) const;

public:
    explicit Am2302Sensor(gpio_num_t pin) : pin_(pin) {}

    entities::Task<std::optional<entities::AirCondition>> Measure() override;
};

}   // namespace frameworksanddrivers
