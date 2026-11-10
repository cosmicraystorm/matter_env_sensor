#pragma once

#include <app/icd/server/ICDStateObserver.h>
#include "../entities/AsyncSignal.hpp"
#include "../usecases/StatusIndicator.h"

namespace frameworksanddrivers {

/// @brief ICD(SED) の Active/Idle 遷移を受けて、LED表示と測定起床を行う
/// コールバックは CHIP スレッドで呼ばれる
class MatterIcdObserver : public chip::app::ICDStateObserver {
private:
    usecases::StatusIndicator& statusIndicator_;
    entities::AsyncSignal& wakeSignal_;

public:
    MatterIcdObserver(usecases::StatusIndicator& statusIndicator, entities::AsyncSignal& wakeSignal)
        : statusIndicator_(statusIndicator), wakeSignal_(wakeSignal) {}

    /// @brief Idle → Active (SED 復帰)
    void OnEnterActiveMode() override {
        statusIndicator_.OnWake();
        wakeSignal_.Set();
    }

    /// @brief Active → Idle (SED スリープ)
    void OnEnterIdleMode() override {
        statusIndicator_.OnSleep();
    }

    void OnTransitionToIdle() override {}
    void OnICDModeChange() override {}
};

}   // namespace frameworksanddrivers
