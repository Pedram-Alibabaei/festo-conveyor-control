#include "ServiceState.hpp"
#include "IdleState.hpp"
#include "logik/logik.hpp"
#include "logik/fsm/level1/Level1Actions.hpp"
#include <iostream>

namespace logik {

void ServiceState::entry() {
    std::cout << "[ServiceState] Entered (Ebene 1). Kalibrierungsmodus aktiv." << std::endl;

    if (m_context == nullptr) {
        return;
    }

    level1::applyServiceEntry(m_context->getDispatcher(), m_context->getEventManager());

    m_sub = createInitialServiceCalibSubState();
    if (m_sub != nullptr) {
        m_sub->entry(m_context, *this);
    }
}

void ServiceState::exit() {
    if (m_sub != nullptr) {
        m_sub->exit(m_context);
        m_sub.reset();
    }

    if (m_context == nullptr) {
        return;
    }

    level1::applyServiceExit(m_context->getDispatcher(), m_context->getEventManager());
}

void ServiceState::replaceSubState(std::unique_ptr<ServiceCalibSubStateBase> next) {
    if (m_sub != nullptr) {
        m_sub->exit(m_context);
    }

    m_sub = std::move(next);
    if (m_sub != nullptr && m_context != nullptr) {
        m_sub->entry(m_context, *this);
    }
}

void ServiceState::handle(Event e) {
    if (!m_context) {
        return;
    }

    switch (e) {
    case Event::STOP_PRESSED:
        std::cout << "[ServiceState] STOP -> IdleState" << std::endl;
        m_context->transitionTo(new IdleState());
        return;

    case Event::DATA_EXIST:
        if (auto* eventManager = m_context->getEventManager()) {
            eventManager->setCalibrationDataReady(true);
        }
        std::cout << "[ServiceState] Kalibrierung gespeichert / vorhanden." << std::endl;
        return;

    case Event::MOTOR_RUN:
    case Event::MOTOR_STOP:
    case Event::SET_SLOW:
        // Motor-Steuerung uebernimmt die Service-Sub-FSM.
        return;

    case Event::RESET_PRESSED:
    case Event::PARTNER_RESET_PRESSED:
        // Reset wird in Calib_Finished ausgewertet.
        break;

    default:
        break;
    }

    if (m_sub != nullptr) {
        m_sub->handle(e, m_context, *this);
    }
}

} // namespace logik
