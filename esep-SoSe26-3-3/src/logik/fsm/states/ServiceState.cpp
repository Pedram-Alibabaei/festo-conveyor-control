#include "ServiceState.hpp"
#include "IdleState.hpp"
#include "logik/fsm/states/service/ServiceCalibRegistry.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/fsm/level1/Level1Actions.hpp"
#include <iostream>

namespace logik {

void ServiceState::entry() {
    std::cout << "[ServiceState] Entered (Ebene 1). Kalibrierungsmodus aktiv." << std::endl;

    if (m_context == nullptr) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();
    auto* eventManager = m_context->getEventManager();

    if (ServiceCalibRegistry::hasSavedPhase()) {
        const auto phase = ServiceCalibRegistry::savedPhase();
        std::cout << "[ServiceState] Kalibrierung nach ConnectionLost fortsetzen (Phase="
                  << static_cast<int>(phase) << ")." << std::endl;

        level1::applyServiceResumeEntry(dispatcher, eventManager);

        if (dispatcher != nullptr
            && ServiceCalibRegistry::needsServiceTimingAdc(phase)) {
            dispatcher->setServiceTimingMeasureState();
        }

        ServiceCalibRegistry::setResuming(true);
        m_sub = ServiceCalibRegistry::createSavedSubState();
        ServiceCalibRegistry::clear();
        if (m_sub != nullptr) {
            m_sub->entry(m_context, *this);
        }
        ServiceCalibRegistry::setResuming(false);
        return;
    }

    level1::applyServiceEntry(dispatcher, eventManager);

    m_sub = createInitialServiceCalibSubState();
    if (m_sub != nullptr) {
        m_sub->entry(m_context, *this);
    }
}

void ServiceState::exit() {
    if (m_sub != nullptr) {
        if (m_context != nullptr) {
            auto* eventManager = m_context->getEventManager();
            if (eventManager != nullptr && eventManager->isConnectionLost()) {
                ServiceCalibRegistry::savePhase(m_sub.get());
                std::cout << "[ServiceState] Kalibrierungs-Phase gesichert fuer Restore."
                          << std::endl;
                m_sub->exit(m_context);
                m_sub.reset();
                return;
            }
        }

        m_sub->exit(m_context);
        m_sub.reset();
    }

    if (m_context == nullptr) {
        return;
    }

    level1::applyServiceExit(m_context->getDispatcher(), m_context->getEventManager());
    ServiceCalibRegistry::clear();
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
    case Event::STOP_RELEASED:
        std::cout << "[ServiceState] STOP -> IdleState" << std::endl;
        m_context->transitionTo(new IdleState());
        return;

    case Event::START_LONG_PRESSED:
        std::cout << "[ServiceState] START lang (neu kalibrieren)" << std::endl;
        return;

    case Event::DATA_EXIST:
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
