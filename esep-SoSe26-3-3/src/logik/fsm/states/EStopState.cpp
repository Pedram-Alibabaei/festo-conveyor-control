#include "EStopState.hpp"
#include "IdleState.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void EStopState::onEnter() {
    std::cout << "[EStopState] Entered. Sicherer Zustand eingenommen!" << std::endl;
    m_isHardwareUnlocked = false;

    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();
    dispatcher->driveStop();
    dispatcher->turnAllOff();
    dispatcher->lightRedBlinkFast();

    if (auto* em = m_context->getEventManager()) {
        em->setMotorOn(false);
        em->setMotorSlow(false);
        em->setEStopActive(true);
        em->setServiceMode(false);
    }
}

void EStopState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();

    switch (e) {
    case Event::ESTOP_RELEASED:
    case Event::PARTNER_ESTOP_RELEASED:
        std::cout << "[EStopState] E-Stop entriegelt -> Warte auf RESET." << std::endl;
        m_isHardwareUnlocked = true;
        dispatcher->turnLEDQ2Off();
        dispatcher->lightRedOn();
        break;

    case Event::ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_PRESSED:
        std::cout << "[EStopState] E-Stop erneut verriegelt!" << std::endl;
        m_isHardwareUnlocked = false;
        dispatcher->lightRedBlinkFast();
        break;

    case Event::RESET_PRESSED:
    case Event::PARTNER_RESET_PRESSED:
        if (m_isHardwareUnlocked) {
            std::cout << "[EStopState] RESET quittiert -> IdleState" << std::endl;
            dispatcher->lightRedOff();
            dispatcher->lightYellowOff();
            if (auto* em = m_context->getEventManager()) {
                em->setEStopActive(false);
                em->setErrorActive(false);
            }
            m_context->transitionTo(new IdleState());
        } else {
            std::cout << "[EStopState] RESET blockiert: E-Stop noch aktiv!" << std::endl;
        }
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
