#include "logik/fsm/common/BaseState.hpp"
#include "Level0Actions.hpp"
#include "L0RestoreRegistry.hpp"
#include "ErrorState.hpp"
#include "logik/fsm/level1/L1StateRegistry.hpp"
#include "logik/fsm/level1/ReadyForActiveState.hpp"
#include "logik/fsm/level0/ActiveRegionState.hpp"
#include "logik/fsm/level1/IdleState.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include <iostream>
#include "logik/workpieceManager/WorkpieceManager.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <memory>

namespace logik {
namespace level0 {

void applyConnectionLostEntry(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->driveStop();
    dispatcher->closeSwitch();
    dispatcher->lightRedBlinkFast();
    dispatcher->lightYellowBlinkFast();
    dispatcher->lightGreenBlinkFast();

    const bool inService = eventManager != nullptr && eventManager->isServiceMode();
    if (inService) {
        std::cout << "[Level0] ConnectionLost: Service-Modus – ADC-Zustand beibehalten."
                  << std::endl;
    } else {
        dispatcher->setAdcIdelState();
    }
}

void applyConnectionLostEstopWaitingResetEntry(I_Dispatcher* dispatcher) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->turnLEDQ2Off();
    dispatcher->lightYellowBlinkFast();
    dispatcher->lightGreenBlinkFast();
    dispatcher->lightRedOn();
    dispatcher->turnLEDResetBlinkFast();
}

void clearTrafficLightsForRestore(I_Dispatcher* dispatcher) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->lightRedOff();
    dispatcher->lightYellowOff();
    dispatcher->lightGreenOff();
}

void transitionAfterConnectionRestore(Logik* logik) {
    if (logik == nullptr) {
        return;
    }

    clearTrafficLightsForRestore(logik->getDispatcher());

    switch (L0RestoreRegistry::target()) {
    case L0RestoreTarget::ERROR_PENDING_UNACK:
        std::cout << "[Level0] RESTORED -> ErrorState (anstehend unquittiert)" << std::endl;
        logik->transitionTo(new ErrorState(ErrorState::SubState::PendingUnacknowledged));
        break;

    case L0RestoreTarget::ERROR_PENDING_ACK:
        std::cout << "[Level0] RESTORED -> ErrorState (anstehend quittiert)" << std::endl;
        logik->transitionTo(new ErrorState(ErrorState::SubState::PendingAcknowledged));
        break;

    case L0RestoreTarget::ERROR_GONE_UNACK:
        std::cout << "[Level0] RESTORED -> ErrorState (gegangen unquittiert)" << std::endl;
        logik->transitionTo(new ErrorState(ErrorState::SubState::GoneUnacknowledged));
        break;

    case L0RestoreTarget::ACTIVE_REGION:
    default: {
        const L1StateId history = L1StateRegistry::history();
        if (history == L1StateId::RUNNING || history == L1StateId::READY_FOR_ACTIVE) {
            std::cout << "[Level0] RESTORED -> ReadyForActive "
                      << "(Running pausiert, START zum Fortsetzen)" << std::endl;
            logik->transitionTo(
                new ActiveRegionState(std::unique_ptr<BaseState>(new ReadyForActiveState())));
        } else {
            std::cout << "[Level0] RESTORED -> ActiveRegion (H*, L1="
                      << static_cast<int>(history) << ")" << std::endl;
            logik->transitionTo(new ActiveRegionState(ActiveRegionState::EntryMode::RestoreHistory));
        }
        break;
    }
    }

    L0RestoreRegistry::clear();
}

void applyEstopPressedTraffic(I_Dispatcher* dispatcher) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->lightYellowOff();
    dispatcher->lightGreenOff();
    dispatcher->lightRedBlinkFast();
}

void applyEstopLocalActiveEntry(Logik* logik) {
    if (logik == nullptr) {
        return;
    }

    auto* dispatcher = logik->getDispatcher();
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->driveStop();
    dispatcher->closeSwitch();
    dispatcher->setAdcIdelState();
    dispatcher->lightYellowOff();
    dispatcher->lightGreenOff();
    dispatcher->lightRedBlinkFast();
    dispatcher->turnLEDQ2BlinkFast();

    if (auto* wp = logik->getWorkpieceManager()) {
        wp->clearAll();
    }
}

void applyEstopPartnerActiveEntry(Logik* logik) {
    if (logik == nullptr) {
        return;
    }

    auto* dispatcher = logik->getDispatcher();
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->driveStop();
    dispatcher->closeSwitch();
    dispatcher->setAdcIdelState();
    dispatcher->lightYellowOff();
    dispatcher->lightGreenOff();
    dispatcher->lightRedBlinkFast();
    dispatcher->turnLEDQ2Off();

    if (auto* wp = logik->getWorkpieceManager()) {
        wp->clearAll();
    }
}

void applyEstopBothNotActiveEntry(I_Dispatcher* dispatcher) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->turnLEDQ2Off();
    dispatcher->lightYellowOff();
    dispatcher->lightGreenOff();
    dispatcher->lightRedOn();
    dispatcher->turnLEDResetBlinkFast();
}

void syncLocalEstopPanelLed(I_Dispatcher* dispatcher) {
    if (dispatcher == nullptr) {
        return;
    }

    if (dispatcher->isEstopPressed()) {
        dispatcher->turnLEDQ2BlinkFast();
    } else {
        dispatcher->turnLEDQ2Off();
    }
}

bool isLocalEstopActive(I_Dispatcher* dispatcher) {
    return dispatcher != nullptr && dispatcher->isEstopPressed();
}

bool isPartnerEstopActive(Logik* logik) {
    if (logik == nullptr) {
        return false;
    }

    auto* dispatcher = logik->getDispatcher();
    return dispatcher != nullptr && dispatcher->isPartnerEstopPressed();
}

void finishEstopReset(Logik* logik) {
    if (logik == nullptr) {
        return;
    }

    std::cout << "[EStop] Beide Reset gedrueckt -> Rot aus, zurueck Active/Idle." << std::endl;

    if (auto* dispatcher = logik->getDispatcher()) {
        dispatcher->turnLEDResetOff();
        dispatcher->lightRedOff();
        dispatcher->turnLEDQ2Off();
    }

    if (auto* em = logik->getEventManager()) {
        em->setEStopActive(false);
        em->setErrorActive(false);
        em->setRunningMode(false);
    }

    L1StateRegistry::setHistory(L1StateId::IDLE);
    logik->transitionTo(
        new ActiveRegionState(std::unique_ptr<BaseState>(new IdleState())));
}

void applyEstopSafetyClear(Logik* logik) {
    if (logik == nullptr) {
        return;
    }

    if (auto* wp = logik->getWorkpieceManager()) {
        wp->clearAll();
    }

    if (auto* em = logik->getEventManager()) {
        em->setChuteFull(false);
        em->setPartnerChuteStatus(false);
        em->setRunningMode(false);
    }
}

bool isAnyEstopActive(Logik* logik) {
    if (logik == nullptr) {
        return false;
    }

    auto* em = logik->getEventManager();
    auto* dispatcher = logik->getDispatcher();

    if (em != nullptr && em->isEStopActive()) {
        return true;
    }

    if (dispatcher != nullptr) {
        return dispatcher->isEstopPressed() || dispatcher->isPartnerEstopPressed();
    }

    return false;
}

} // namespace level0
} // namespace logik
