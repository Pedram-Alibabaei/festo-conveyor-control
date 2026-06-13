#include "EStopState.hpp"
#include "ActiveRegionState.hpp"
#include "ConnectionLostAndEStopState.hpp"
#include "IdleState.hpp"
#include "logik/fsm/level0/L1StateRegistry.hpp"
#include "logik/fsm/level0/Level0Actions.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void EStopState::entry() {
    std::cout << "[EStopState] Entered (Ebene 0). Sicherer Zustand!" << std::endl;
    m_isHardwareUnlocked = false;

    if (m_context == nullptr) {
        return;
    }

    level0::applyEstopSafetyClear(m_context);

    if (auto* dispatcher = m_context->getDispatcher()) {
        dispatcher->driveStop();
        dispatcher->closeSwitch();
        dispatcher->turnAllOff();
        dispatcher->lightRedBlinkFast();
        dispatcher->setAdcIdelState();
    }

    if (auto* em = m_context->getEventManager()) {
        em->setMotorOn(false);
        em->setMotorSlow(false);
        em->setEStopActive(true);
        em->setServiceMode(false);
    }

    L1StateRegistry::setHistory(L1StateId::IDLE);
}

void EStopState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();
    auto* em = m_context->getEventManager();

    switch (e) {
    case Event::CONNECTION_LOST:
        std::cout << "[EStopState] CONNECTION_LOST -> ConnectionLostAndEStop" << std::endl;
        dispatcher->lightYellowBlinkFast();
        dispatcher->lightGreenBlinkFast();
        m_context->transitionTo(new ConnectionLostAndEStopState());
        break;

    case Event::CONNECTION_RESTORED:
        std::cout << "[EStopState] CONNECTION_RESTORED (bleibt in Estop)" << std::endl;
        dispatcher->lightYellowOff();
        dispatcher->lightGreenOff();
        break;

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
            std::cout << "[EStopState] RESET -> ActiveRegion/Idle (kein H*)" << std::endl;
            dispatcher->lightRedOff();
            dispatcher->lightYellowOff();
            if (em != nullptr) {
                em->setEStopActive(false);
                em->setErrorActive(false);
            }
            L1StateRegistry::setHistory(L1StateId::IDLE);
            m_context->transitionTo(
                new ActiveRegionState(std::unique_ptr<BaseState>(new IdleState())));
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
