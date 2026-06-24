#include "EStopState.hpp"
#include "ActiveRegionState.hpp"
#include "ConnectionLostAndEStopState.hpp"
#include "logik/fsm/level1/IdleState.hpp"
#include "logik/fsm/level1/L1StateRegistry.hpp"
#include "logik/fsm/level0/Level0Actions.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void EStopState::clearResetLatches() {
    m_localResetLatched = false;
    m_partnerResetLatched = false;
}

void EStopState::latchLocalReset(Logik* context) {
    if (m_localResetLatched) {
        return;
    }

    m_localResetLatched = true;
    std::cout << "[EStopState] Lokaler Reset quittiert (latched)." << std::endl;

    if (context != nullptr && context->getDispatcher() != nullptr) {
        context->getDispatcher()->sendPartnerResetPressed();
        context->getDispatcher()->turnLEDResetOn();
    }
}

void EStopState::latchPartnerReset(Logik* context) {
    if (m_partnerResetLatched) {
        return;
    }

    m_partnerResetLatched = true;
    std::cout << "[EStopState] Partner-Reset quittiert (latched)." << std::endl;
}

void EStopState::applyResetWaitingVisuals(Logik* context) {
    if (context == nullptr || context->getDispatcher() == nullptr) {
        return;
    }

    auto* dispatcher = context->getDispatcher();
    dispatcher->lightRedOn();
    if (m_localResetLatched) {
        dispatcher->turnLEDResetOn();
    } else {
        dispatcher->turnLEDResetBlinkFast();
    }
}

void EStopState::tryFinishReset() {
    if (!m_localResetLatched || !m_partnerResetLatched) {
        std::cout << "[EStopState] Reset wartet: local="
                  << m_localResetLatched << " partner=" << m_partnerResetLatched << std::endl;
        return;
    }

    completeResetAndLeave();
}

void EStopState::replaceSubState(std::unique_ptr<EStopSubStateBase> next) {
    if (m_sub != nullptr && m_context != nullptr) {
        m_sub->exit(m_context);
    }

    m_sub = std::move(next);
    if (m_sub != nullptr && m_context != nullptr) {
        m_sub->entry(m_context, *this);
    }
}

void EStopState::completeResetAndLeave() {
    if (m_sub != nullptr && m_context != nullptr) {
        m_sub->exit(m_context);
    }
    m_sub.reset();
    clearResetLatches();

    if (m_context != nullptr) {
        level0::finishEstopReset(m_context);
    }
}

void EStopState::entry() {
    std::cout << "[EStopState] Entered (Ebene 0). Sicherer Zustand!" << std::endl;
    clearResetLatches();

    if (m_context == nullptr) {
        return;
    }

    level0::applyEstopSafetyClear(m_context);

    if (auto* dispatcher = m_context->getDispatcher()) {
        dispatcher->turnAllOff();
    }

    if (auto* em = m_context->getEventManager()) {
        em->setRunningMode(false);
        em->setMotorOn(false);
        em->setMotorSlow(false);
        em->setEStopActive(true);
        em->setServiceMode(false);
    }

    L1StateRegistry::setHistory(L1StateId::IDLE);
    replaceSubState(createInitialEStopSubState(m_context));
}

void EStopState::exit() {
    if (m_sub != nullptr && m_context != nullptr) {
        m_sub->exit(m_context);
    }
    m_sub.reset();
    clearResetLatches();
}

void EStopState::handle(Event e) {
    if (m_context == nullptr) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();

    switch (e) {
    case Event::CONNECTION_LOST:
        std::cout << "[EStopState] CONNECTION_LOST -> ConnectionLostAndEStop" << std::endl;
        if (dispatcher != nullptr) {
            dispatcher->lightYellowBlinkFast();
            dispatcher->lightGreenBlinkFast();
        }
        m_context->transitionTo(new ConnectionLostAndEStopState());
        return;

    case Event::CONNECTION_RESTORED:
        std::cout << "[EStopState] CONNECTION_RESTORED -> Unterzustand neu synchronisieren" << std::endl;
        clearResetLatches();
        replaceSubState(createInitialEStopSubState(m_context));
        return;

    case Event::RESET_RELEASED:
    case Event::PARTNER_RESET_RELEASED:
        break;

    default:
        break;
    }

    if (m_sub != nullptr) {
        m_sub->handle(e, m_context, *this);
    }
}

} // namespace logik
