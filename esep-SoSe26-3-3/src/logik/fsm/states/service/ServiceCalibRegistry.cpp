#include "ServiceCalibRegistry.hpp"
#include "ServiceCalibSubState.hpp"

namespace logik {

namespace {
ServiceCalibPhase s_savedPhase = ServiceCalibPhase::None;
bool s_resuming = false;
} // namespace

bool ServiceCalibRegistry::hasSavedPhase() {
    return s_savedPhase != ServiceCalibPhase::None;
}

ServiceCalibPhase ServiceCalibRegistry::savedPhase() {
    return s_savedPhase;
}

void ServiceCalibRegistry::savePhase(const ServiceCalibSubStateBase* sub) {
    if (sub == nullptr) {
        s_savedPhase = ServiceCalibPhase::None;
        return;
    }

    if (dynamic_cast<const ServiceInitEmptySubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::InitEmpty;
    } else if (dynamic_cast<const ServiceMotorRightSlowSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::MotorRightSlow;
    } else if (dynamic_cast<const ServiceSlowResetTickSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::SlowResetTick;
    } else if (dynamic_cast<const ServiceWaitHeightClearedSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::WaitHeightCleared;
    } else if (dynamic_cast<const ServiceWaitEjectorBlockedSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::WaitEjectorBlocked;
    } else if (dynamic_cast<const ServiceWaitEjectorClearedSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::WaitEjectorCleared;
    } else if (dynamic_cast<const ServiceWaitChuteBlockedSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::WaitChuteBlocked;
    } else if (dynamic_cast<const ServiceSlowCalibDoneSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::SlowCalibDone;
    } else if (dynamic_cast<const ServiceNormalStartSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::NormalStart;
    } else if (dynamic_cast<const ServiceNormalWaitHeightClearedSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::NormalWaitHeightCleared;
    } else if (dynamic_cast<const ServiceNormalWaitEjectorBlockedSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::NormalWaitEjectorBlocked;
    } else if (dynamic_cast<const ServiceNormalWaitEjectorClearedSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::NormalWaitEjectorCleared;
    } else if (dynamic_cast<const ServiceNormalWaitEndBlockedSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::NormalWaitEndBlocked;
    } else if (dynamic_cast<const ServiceWaitWpRemovedSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::WaitWpRemoved;
    } else if (dynamic_cast<const ServiceCalibFinishedSubState*>(sub) != nullptr) {
        s_savedPhase = ServiceCalibPhase::CalibFinished;
    } else {
        s_savedPhase = ServiceCalibPhase::None;
    }
}

void ServiceCalibRegistry::clear() {
    s_savedPhase = ServiceCalibPhase::None;
}

std::unique_ptr<ServiceCalibSubStateBase> ServiceCalibRegistry::createSavedSubState() {
    switch (s_savedPhase) {
    case ServiceCalibPhase::InitEmpty:
        return std::make_unique<ServiceInitEmptySubState>();
    case ServiceCalibPhase::MotorRightSlow:
        return std::make_unique<ServiceMotorRightSlowSubState>();
    case ServiceCalibPhase::SlowResetTick:
        return std::make_unique<ServiceSlowResetTickSubState>();
    case ServiceCalibPhase::WaitHeightCleared:
        return std::make_unique<ServiceWaitHeightClearedSubState>();
    case ServiceCalibPhase::WaitEjectorBlocked:
        return std::make_unique<ServiceWaitEjectorBlockedSubState>();
    case ServiceCalibPhase::WaitEjectorCleared:
        return std::make_unique<ServiceWaitEjectorClearedSubState>();
    case ServiceCalibPhase::WaitChuteBlocked:
        return std::make_unique<ServiceWaitChuteBlockedSubState>();
    case ServiceCalibPhase::SlowCalibDone:
        return std::make_unique<ServiceSlowCalibDoneSubState>();
    case ServiceCalibPhase::NormalStart:
        return std::make_unique<ServiceNormalStartSubState>();
    case ServiceCalibPhase::NormalWaitHeightCleared:
        return std::make_unique<ServiceNormalWaitHeightClearedSubState>();
    case ServiceCalibPhase::NormalWaitEjectorBlocked:
        return std::make_unique<ServiceNormalWaitEjectorBlockedSubState>();
    case ServiceCalibPhase::NormalWaitEjectorCleared:
        return std::make_unique<ServiceNormalWaitEjectorClearedSubState>();
    case ServiceCalibPhase::NormalWaitEndBlocked:
        return std::make_unique<ServiceNormalWaitEndBlockedSubState>();
    case ServiceCalibPhase::WaitWpRemoved:
        return std::make_unique<ServiceWaitWpRemovedSubState>();
    case ServiceCalibPhase::CalibFinished:
        return std::make_unique<ServiceCalibFinishedSubState>();
    case ServiceCalibPhase::None:
    default:
        return nullptr;
    }
}

bool ServiceCalibRegistry::isResuming() {
    return s_resuming;
}

void ServiceCalibRegistry::setResuming(bool value) {
    s_resuming = value;
}

bool ServiceCalibRegistry::needsServiceTimingAdc(ServiceCalibPhase phase) {
    switch (phase) {
    case ServiceCalibPhase::NormalStart:
    case ServiceCalibPhase::NormalWaitHeightCleared:
    case ServiceCalibPhase::NormalWaitEjectorBlocked:
    case ServiceCalibPhase::NormalWaitEjectorCleared:
    case ServiceCalibPhase::NormalWaitEndBlocked:
    case ServiceCalibPhase::WaitWpRemoved:
    case ServiceCalibPhase::CalibFinished:
        return true;
    default:
        return false;
    }
}

} // namespace logik
