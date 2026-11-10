#pragma once

#include "HeldOutputPin.hpp"
#include "PinAssign.h"

namespace frameworksanddrivers {

/// @brief XIAO ESP32C6 固有の初期化
/// Arduino では variant の initVariant() が行っている RF スイッチ設定を、ESP-IDF では自前で行う
inline void InitXiaoRfSwitch() {
    static HeldOutputPin rfSwitchEnable(RF_SWITCH_ENABLE_PIN);
    static HeldOutputPin antennaSelect(RF_ANTENNA_SELECT_PIN);
    rfSwitchEnable.Begin(0);    // RFスイッチ有効
    antennaSelect.Begin(0);     // 内蔵アンテナ
}

}   // namespace frameworksanddrivers
