#include "L0RestoreRegistry.hpp"
#include "logik/fsm/common/BaseState.hpp"
#include "logik/fsm/level0/ActiveRegionState.hpp"
#include "logik/fsm/level0/ErrorState.hpp"
#include "shared/detailLog.hpp"
#include <iostream>

namespace logik {
namespace level0 {

L0RestoreTarget L0RestoreRegistry::s_target = L0RestoreTarget::ACTIVE_REGION;

void L0RestoreRegistry::captureFrom(const BaseState* stateBeforeConnectionLost) {
    if (stateBeforeConnectionLost == nullptr) {
        s_target = L0RestoreTarget::ACTIVE_REGION;
        return;
    }

    if (const auto* errorState = dynamic_cast<const ErrorState*>(stateBeforeConnectionLost)) {
        switch (errorState->getSubState()) {
        case ErrorState::SubState::PendingAcknowledged:
            s_target = L0RestoreTarget::ERROR_PENDING_ACK;
            break;
        case ErrorState::SubState::GoneUnacknowledged:
            s_target = L0RestoreTarget::ERROR_GONE_UNACK;
            break;
        case ErrorState::SubState::PendingUnacknowledged:
        default:
            s_target = L0RestoreTarget::ERROR_PENDING_UNACK;
            break;
        }
        DETAIL_LOG( "[L0Restore] Snapshot vor CONNECTION_LOST: Error (SubState="
                  << static_cast<int>(errorState->getSubState()) << ").");
        return;
    }

    if (dynamic_cast<const ActiveRegionState*>(stateBeforeConnectionLost) != nullptr) {
        s_target = L0RestoreTarget::ACTIVE_REGION;
        DETAIL_LOG( "[L0Restore] Snapshot vor CONNECTION_LOST: ActiveRegion (L1-H*)."
                 );
        return;
    }

    s_target = L0RestoreTarget::ACTIVE_REGION;
    DETAIL_LOG( "[L0Restore] Snapshot vor CONNECTION_LOST: Fallback ActiveRegion."
             );
}

L0RestoreTarget L0RestoreRegistry::target() {
    return s_target;
}

void L0RestoreRegistry::clear() {
    s_target = L0RestoreTarget::ACTIVE_REGION;
}

} // namespace level0
} // namespace logik
