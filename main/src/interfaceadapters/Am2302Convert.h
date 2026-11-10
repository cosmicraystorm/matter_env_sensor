#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include "../entities/AirCondition.h"

namespace interfaceadapters {

/// @brief AM2302の受信データ(湿度H/L, 温度H/L, チェックサム)
using Am2302RawData = std::array<std::uint8_t, 5>;

/// @brief AM2302の受信データを温湿度に変換する
/// @return チェックサム不一致時は std::nullopt
constexpr std::optional<entities::AirCondition> DecodeAm2302(const Am2302RawData& raw) {
    const std::uint8_t checksum = static_cast<std::uint8_t>(raw[0] + raw[1] + raw[2] + raw[3]);
    if (checksum != raw[4]) {
        return std::nullopt;
    }

    const double humidity = static_cast<double>((raw[0] << 8) | raw[1]) / 10.0;
    // 温度は最上位ビットが符号、残り15ビットが絶対値(×10)
    const double temperatureAbs = static_cast<double>(((raw[2] & 0x7F) << 8) | raw[3]) / 10.0;
    const double temperature = (raw[2] & 0x80) ? -temperatureAbs : temperatureAbs;

    return entities::AirCondition{temperature, humidity};
}

// データシート記載の例で変換を検証
static_assert(DecodeAm2302({0x02, 0x92, 0x01, 0x0D, 0xA2})->humidityPercent == 65.8);
static_assert(DecodeAm2302({0x02, 0x92, 0x01, 0x0D, 0xA2})->temperatureCelsius == 26.9);
static_assert(DecodeAm2302({0x02, 0x92, 0x80, 0x65, 0x79})->temperatureCelsius == -10.1);
static_assert(!DecodeAm2302({0x02, 0x92, 0x01, 0x0D, 0x00}).has_value());

}   // namespace interfaceadapters
