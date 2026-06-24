#include "ErrorState.hpp"
#include "ActiveRegionState.hpp"
#include "ConnectionLostState.hpp"
#include "EStopState.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/workpieceManager/WorkpieceManager.hpp"
#include "logik/errorHandler/errorHandler.hpp"
#include "logik/fsm/level1/Level1Actions.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

ErrorState::ErrorState(SubState restoredSubState)
    : m_subState(restoredSubState),
      m_restoreEntry(true)
{
}

void ErrorState::entry() {
    std::cout << "[ErrorState] Entered (Ebene 0)." << std::endl;

    if (m_restoreEntry) {
        m_restoreEntry = false;
        switch (m_subState) {
        case SubState::PendingAcknowledged:
            enterPendingAcknowledged();
            std::cout << "[ErrorState] Wiederhergestellt nach CONNECTION_RESTORED "
                      << "(anstehend quittiert)." << std::endl;
            break;
        case SubState::GoneUnacknowledged:
            m_errorCauseCleared = true;
            enterGoneUnacknowledged();
            std::cout << "[ErrorState] Wiederhergestellt nach CONNECTION_RESTORED "
                      << "(gegangen unquittiert)." << std::endl;
            break;
        case SubState::PendingUnacknowledged:
        default:
            enterPendingUnacknowledged();
            std::cout << "[ErrorState] Wiederhergestellt nach CONNECTION_RESTORED "
                      << "(anstehend unquittiert)." << std::endl;
            break;
        }
        return;
    }

    enterPendingUnacknowledged();
}

void ErrorState::exit() {
    if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
        auto* dispatcher = m_context->getDispatcher();
        dispatcher->lightRedOff();
        dispatcher->lightGreenOff();
        dispatcher->turnLEDResetOff();
        dispatcher->turnLEDStartOff();
    }
}

void ErrorState::onErrorCauseCleared() {
    m_errorCauseCleared = true;
    if (m_subState == SubState::PendingAcknowledged
        && m_context != nullptr
        && m_context->getDispatcher() != nullptr) {
        m_context->getDispatcher()->turnLEDStartBlinkSlow();
        std::cout << "[ErrorState] Fehlerursache behoben – START fuer H* freigegeben."
                  << std::endl;
    }
}

bool ErrorState::canLeaveErrorWithStart() const {
    if (!m_errorCauseCleared) {
        return false;
    }
    if (m_context == nullptr || m_context->getErrorHandler() == nullptr) {
        return m_errorCauseCleared;
    }
    return m_context->getErrorHandler()->isErrorCauseCleared();
}

void ErrorState::enterPendingUnacknowledged() {
    m_subState = SubState::PendingUnacknowledged;
    m_errorCauseCleared = false;

    if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
        auto* dispatcher = m_context->getDispatcher();
        dispatcher->driveStop();
        dispatcher->lightGreenOff();
        dispatcher->lightRedBlinkFast();
        dispatcher->turnLEDResetBlinkFast();
        dispatcher->turnLEDStartOff();
    }

    if (m_context != nullptr && m_context->getEventManager() != nullptr) {
        m_context->getEventManager()->setErrorActive(true);
        m_context->getEventManager()->setRunningMode(false);
        m_context->getEventManager()->setMotorOn(false);
        m_context->getEventManager()->setMotorSlow(false);
    }

    std::cout << "[ErrorState] Anstehend unquittiert – Rot schnell blinkend "
              << "(Fehler noch aktiv)" << std::endl;
}

void ErrorState::enterPendingAcknowledged() {
    m_subState = SubState::PendingAcknowledged;

    if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
        auto* dispatcher = m_context->getDispatcher();
        dispatcher->lightRedOn();
        dispatcher->turnLEDResetOn();
        if (m_errorCauseCleared) {
            dispatcher->turnLEDStartBlinkSlow();
        } else {
            dispatcher->turnLEDStartOff();
        }
    }

    if (m_context != nullptr && m_context->getEventManager() != nullptr) {
        m_context->getEventManager()->setErrorActive(true);
        m_context->getEventManager()->setRunningMode(false);
        m_context->getEventManager()->setMotorOn(false);
        m_context->getEventManager()->setMotorSlow(false);
    }

    std::cout << "[ErrorState] Anstehend quittiert – Rot dauerhaft. "
              << "START erst nach behobener Fehlerursache (ERROR_CLEARED)."
              << std::endl;
}

void ErrorState::enterGoneUnacknowledged() {
    m_subState = SubState::GoneUnacknowledged;
    m_errorCauseCleared = true;

    if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
        auto* dispatcher = m_context->getDispatcher();
        dispatcher->lightRedBlinkSlow();
        dispatcher->turnLEDResetBlinkFast();
        dispatcher->turnLEDStartOff();
    }

    std::cout << "[ErrorState] Gegangen unquittiert – Rot langsam blinkend "
              << "(Fehler behoben, RESET quittieren)" << std::endl;
}

void ErrorState::leaveErrorState() {
    if (m_context == nullptr) {
        return;
    }

    if (auto* eventManager = m_context->getEventManager()) {
        eventManager->setErrorActive(false);
    }

    if (auto* errorHandler = m_context->getErrorHandler()) {
        errorHandler->resetActiveErrorTracking();
    }

    if (auto* eventManager = m_context->getEventManager()) {
        if (eventManager->context().position == AnlagenPosition::HINTEN_ANLAGE_2) {
            if (auto* wpManager = m_context->getWorkpieceManager()) {
                wpManager->recoverPartnerHandoverAfterError();
            }
        }
    }

    std::cout << "[ErrorState] Fehler abgeschlossen -> ActiveRegion (H*)" << std::endl;
    m_context->transitionTo(
        new ActiveRegionState(ActiveRegionState::EntryMode::RestoreHistory));
}

void ErrorState::handle(Event e) {
    if (m_context != nullptr) {
        switch (e) {
        case Event::CONNECTION_LOST:
            std::cout << "[ErrorState] CONNECTION_LOST -> ConnectionLostState" << std::endl;
            m_context->transitionTo(new ConnectionLostState());
            return;

        case Event::ESTOP_PRESSED:
        case Event::PARTNER_ESTOP_PRESSED:
        case Event::PARTNER_ESTOP_SAFETY_SYNC:
            std::cout << "[ErrorState] ESTOP -> EStopState" << std::endl;
            m_context->transitionTo(new EStopState());
            return;

        default:
            break;
        }
    }

    switch (m_subState) {
    case SubState::PendingUnacknowledged:
        if (e == Event::RESET_RELEASED || e == Event::PARTNER_RESET_RELEASED) {
            enterPendingAcknowledged();
        } else if (e == Event::ERROR_CLEARED) {
            onErrorCauseCleared();
            enterGoneUnacknowledged();
        } else if (e == Event::ERROR) {
            enterPendingUnacknowledged();
        }
        break;

    case SubState::PendingAcknowledged:
        if (e == Event::ERROR_CLEARED) {
            onErrorCauseCleared();
        } else if (e == Event::START_SHORT_PRESSED || e == Event::START_LONG_PRESSED) {
            if (!canLeaveErrorWithStart()) {
                std::cout << "[ErrorState] START ignoriert – Fehlerursache noch nicht "
                          << "behoben (warte auf ERROR_CLEARED / Sensor frei)."
                          << std::endl;
                break;
            }
            leaveErrorState();
        } else if (e == Event::ERROR) {
            enterPendingUnacknowledged();
        }
        break;

    case SubState::GoneUnacknowledged:
        if (e == Event::RESET_RELEASED || e == Event::PARTNER_RESET_RELEASED) {
            enterPendingAcknowledged();
        } else if (e == Event::ERROR_CLEARED) {
            onErrorCauseCleared();
        } else if (e == Event::ERROR) {
            enterPendingUnacknowledged();
        }
        break;
    }
}

} // namespace logik
