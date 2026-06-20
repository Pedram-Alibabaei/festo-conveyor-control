#include "RunningState.hpp"
#include "IdleState.hpp"
#include "logik/logik.hpp"
#include "logik/fsm/level1/Level1Actions.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/workpieceManager/workpieceManager.hpp"
#include "logik/errorHandler/errorHandler.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void RunningState::entry() {
    std::cout << "[RunningState] Entered (Ebene 1)." << std::endl;

    if (m_context == nullptr) {
        return;
    }

    level1::applyRunningEntry(m_context->getDispatcher(), m_context->getEventManager());
    const bool hasActiveWorkpieces =
        m_context->getWorkpieceManager() != nullptr
        && m_context->getWorkpieceManager()->hasActiveWorkpieces();
    if (hasActiveWorkpieces) {
        enterEntryNotFree();
    } else {
        enterEntryFree();
    }

    if (auto* errorHandler = m_context->getErrorHandler()) {
        errorHandler->reconcileChuteWarningOnOperationalEntry();
    }

    EventManager* eventManager = m_context->getEventManager();
    if (eventManager != nullptr && eventManager->isChuteFull()) {
        enterRunningWarning();
    } else {
        enterRunningNormal();
    }
    enterMotorIdle();
}

void RunningState::exit() {
    if (m_context == nullptr) {
        return;
    }

    level1::applyRunningExit(m_context->getDispatcher(), m_context->getEventManager());
}

void RunningState::enterRunningNormal() {
    m_warningState = WarningSubState::Normal;

    if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
        m_context->getDispatcher()->lightYellowOff();
    }

    std::cout << "[RunningState] Running_Normal" << std::endl;
}

void RunningState::enterRunningWarning() {
    m_warningState = WarningSubState::Warning;

    if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
        m_context->getDispatcher()->lightYellowOn();
    }

    std::cout << "[RunningState] Running_Warning" << std::endl;
}

void RunningState::enterEntryFree() {
    m_entryState = EntrySubState::Free;

    if (m_context != nullptr && m_context->getDispatcher() != nullptr
        && m_context->getEventManager() != nullptr
        && m_context->getEventManager()->context().position == AnlagenPosition::VORN_ANLAGE_1) {
        m_context->getDispatcher()->turnLEDQ1On();
    }
    if (m_context != nullptr && m_context->getEventManager() != nullptr) {
        m_context->getEventManager()->setEntryFrei(true);
    }

    std::cout << "[RunningState] Entry_Free" << std::endl;
}

void RunningState::enterEntryNotFree() {
    m_entryState = EntrySubState::NotFree;

    if (m_context != nullptr && m_context->getDispatcher() != nullptr
        && m_context->getEventManager() != nullptr
        && m_context->getEventManager()->context().position == AnlagenPosition::VORN_ANLAGE_1) {
        m_context->getDispatcher()->turnLEDQ1Off();
    }
    if (m_context != nullptr && m_context->getEventManager() != nullptr) {
        m_context->getEventManager()->setEntryFrei(false);
    }

    std::cout << "[RunningState] Entry_Not_Free" << std::endl;
}

void RunningState::enterMotorIdle() {
    m_motorState = MotorSubState::Idle;

    auto* dispatcher = m_context != nullptr ? m_context->getDispatcher() : nullptr;
    auto* eventManager = m_context != nullptr ? m_context->getEventManager() : nullptr;
    level1::applyMotorStop(dispatcher, eventManager, false);
    if (eventManager != nullptr) {
        eventManager->setMotorSlow(false);
    }

    std::cout << "[RunningState] Motor_Idle" << std::endl;
}

void RunningState::enterMotorFast() {
    m_motorState = MotorSubState::Fast;

    auto* dispatcher = m_context != nullptr ? m_context->getDispatcher() : nullptr;
    auto* eventManager = m_context != nullptr ? m_context->getEventManager() : nullptr;
    level1::applyMotorRun(dispatcher, eventManager, false);
    if (eventManager != nullptr) {
        eventManager->setMotorSlow(false);
    }

    std::cout << "[RunningState] Motor_Fast" << std::endl;
}

void RunningState::enterMotorSlow() {
    m_motorState = MotorSubState::Slow;

    auto* dispatcher = m_context != nullptr ? m_context->getDispatcher() : nullptr;
    auto* eventManager = m_context != nullptr ? m_context->getEventManager() : nullptr;
    level1::applyMotorSlow(dispatcher, eventManager);
    if (eventManager != nullptr) {
        eventManager->setMotorOn(true);
    }

    std::cout << "[RunningState] Motor_Slow" << std::endl;
}

void RunningState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    switch (e) {
    case Event::STOP_RELEASED:
        if (m_warningState == WarningSubState::Normal
            && (m_context->getEventManager() == nullptr
                || !m_context->getEventManager()->isChuteFull())) {
            std::cout << "[RunningState] STOP -> IdleState" << std::endl;
            m_context->transitionTo(new IdleState());
        } else {
            std::cout << "[RunningState] STOP ignoriert – Warnung noch aktiv." << std::endl;
        }
        break;

    case Event::WARNING:
        if (m_warningState != WarningSubState::Warning) {
            enterRunningWarning();
        }
        break;

    case Event::WARNING_CLEARED:
        if (m_warningState != WarningSubState::Normal) {
            enterRunningNormal();
        }
        break;

    case Event::ENTRY_FREI:
        if (m_entryState != EntrySubState::Free) {
            enterEntryFree();
        }
        break;

    case Event::ENTRY_NOT_FREI:
        if (m_entryState != EntrySubState::NotFree) {
            enterEntryNotFree();
        }
        break;

    case Event::MOTOR_RUN:
        if (m_motorState == MotorSubState::Idle) {
            enterMotorFast();
        }
        break;

    case Event::MOTOR_STOP:
    case Event::END_BLOCKED:
    case Event::CHUTE_BLOCKED:
    case Event::BAND_EMPTY: {
        const bool handoverMotorActive =
            m_context->getWorkpieceManager() != nullptr
            && m_context->getWorkpieceManager()->isHandoverMotorActive();
        WorkpieceManager* wpManager = m_context->getWorkpieceManager();
        EventManager* eventManager = m_context->getEventManager();
        const bool wpWaitingAtEndOnA1 =
            eventManager != nullptr
            && eventManager->context().position == AnlagenPosition::VORN_ANLAGE_1
            && wpManager != nullptr
            && wpManager->getFirstWorkpieceAt(WpLocation::AT_END) != nullptr
            && !handoverMotorActive;
        const bool forceStopAtEnd =
            (e == Event::END_BLOCKED
                && eventManager != nullptr
                && eventManager->context().position == AnlagenPosition::HINTEN_ANLAGE_2)
            || wpWaitingAtEndOnA1;
        const bool allowMotorStop =
            forceStopAtEnd
            || wpManager == nullptr
            || wpManager->shouldAllowMotorStop();
        if (!handoverMotorActive && allowMotorStop && m_motorState != MotorSubState::Idle) {
            enterMotorIdle();
        }
        if (m_context->getWorkpieceManager() != nullptr
            && !m_context->getWorkpieceManager()->hasActiveWorkpieces()
            && m_context->getEventManager() != nullptr
            && m_context->getEventManager()->context().position == AnlagenPosition::VORN_ANLAGE_1
            && m_entryState != EntrySubState::Free
            && !handoverMotorActive) {
            enterEntryFree();
        }
        break;
    }

    case Event::SET_SLOW:
        if (m_motorState == MotorSubState::Fast) {
            enterMotorSlow();
        }
        break;

    case Event::SET_FAST:
    case Event::HEIGHT_CLEARED:
        if (m_motorState == MotorSubState::Slow) {
            enterMotorFast();
        }
        break;

    default:
        break;
    }
}

} // namespace logik
