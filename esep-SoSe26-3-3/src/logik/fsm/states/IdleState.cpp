#include "IdleState.hpp"
#include "RunningState.hpp"
#include "ServiceState.hpp"
#include "logik/logik.hpp"
#include "logik/fsm/level1/Level1Actions.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void IdleState::entry() {
    std::cout << "[IdleState] Entered (Ebene 1)." << std::endl;

    if (m_context == nullptr) {
        return;
    }

    level1::applyIdleEntry(m_context->getDispatcher(), m_context->getEventManager());
}

void IdleState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();
    auto* eventManager = m_context->getEventManager();

    switch (e) {
    case Event::DATA_EXIST:
        if (eventManager != nullptr) {
            eventManager->setCalibrationDataReady(true);
        }
        m_context->notifyPartnerCalibStatus();
        std::cout << "[IdleState] Kalibrierungsdaten vorhanden." << std::endl;
        break;

    case Event::DATA_NOT_EXIST:
        std::cout << "[IdleState] Keine lokale Kalibrierung. Langer START fuer Service-Modus." << std::endl;
        if (eventManager != nullptr) {
            eventManager->setCalibrationDataReady(false);
        }
        break;

    case Event::START_SHORT_PRESSED:
        if (!level1::canEnterRunning(eventManager)) {
            if (eventManager != nullptr && !eventManager->isCalibrationDataReady()) {
                std::cout << "[IdleState] START kurz ignoriert (lokale Kalibrierung fehlt)." << std::endl;
            } else if (eventManager != nullptr && !eventManager->isPartnerCalibrationReady()) {
                std::cout << "[IdleState] START kurz ignoriert (Partner nicht kalibriert)." << std::endl;
            } else if (eventManager != nullptr && eventManager->isConnectionLost()) {
                std::cout << "[IdleState] START kurz ignoriert (keine Partnerverbindung)." << std::endl;
            } else {
                std::cout << "[IdleState] START kurz ignoriert (Bedingungen nicht erfuellt)." << std::endl;
            }
            break;
        }
        std::cout << "[IdleState] Kurzer Start -> RunningState" << std::endl;
        dispatcher->lightGreenOn();
        m_context->transitionTo(new RunningState());
        break;

    case Event::START_LONG_PRESSED:
        std::cout << "[IdleState] Langer Start -> ServiceState" << std::endl;
        dispatcher->lightGreenBlinkFast();
        m_context->transitionTo(new ServiceState());
        break;

    case Event::STOP_PRESSED:
        dispatcher->driveStop();
        break;

    case Event::STOP_RELEASED:
        break;

    case Event::RESET_PRESSED:
    case Event::PARTNER_RESET_PRESSED:
        break;

    case Event::RESET_RELEASED:
    case Event::PARTNER_RESET_RELEASED:
        dispatcher->lightYellowOff();
        break;

    default:
        break;
    }
}

} // namespace logik
