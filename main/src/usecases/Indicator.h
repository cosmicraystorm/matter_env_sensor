#pragma once

#include <cstdint>
#include <span>

namespace usecases {

/// @brief 点灯/消灯できる表示器の抽象
/// いずれの操作も、実行中の Pulse / RepeatPattern を打ち切ってから行う
class Indicator {
public:
    virtual ~Indicator() = default;
    virtual void TurnOn() = 0;
    virtual void TurnOff() = 0;
    /// @brief 指定時間だけ点灯して自動で消灯する
    virtual void Pulse(std::uint32_t durationMs) = 0;
    /// @brief 点灯パターンを周期的に繰り返す
    /// @param stepsMs 点灯→消灯→点灯…の順の各時間。最後のステップの後は周期の残りを消灯する
    /// @param periodMs 繰り返し周期。各ステップの合計以上にすること
    virtual void RepeatPattern(std::span<const std::uint32_t> stepsMs, std::uint32_t periodMs) = 0;
};

}   // namespace usecases
