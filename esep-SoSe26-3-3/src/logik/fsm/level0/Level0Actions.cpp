#include "Level0Actions.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/workpieceManager/WorkpieceManager.hpp"
#include "dispatcher/i_Dispatcher.hpp"

namespace logik {
namespace level0 {

void applyConnectionLostEntry(I_Dispatcher* dispatcher) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->driveStop();
    dispatcher->closeSwitch();
    dispatcher->lightRedBlinkFast();
    dispatcher->lightYellowBlinkFast();
    dispatcher->lightGreenBlinkFast();
    dispatcher->setAdcIdelState();
}

void clearTrafficLightsForRestore(I_Dispatcher* dispatcher) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->lightRedOff();
    dispatcher->lightYellowOff();
    dispatcher->lightGreenOff();
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
