#include "ReadyForActiveState.hpp"
#include "IdleState.hpp"
#include "RunningState.hpp"
#include "logik/logik.hpp"
#include "logik/fsm/level1/Level1Actions.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/errorHandler/errorHandler.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include "shared/detailLog.hpp"
#include <iostream>

namespace logik {

namespace {

void enterRunningResumed(Logik* context) {
    if (context == nullptr) {
        return;
    }

    if (auto* dispatcher = context->getDispatcher()) {
        dispatcher->sendPartnerStartRunning();
    }

    context->transitionTo(new RunningState());
}

} // namespace

void ReadyForActiveState::entry() {
    DETAIL_LOG( "[ReadyForActive] Entered (Ebene 1). Verbindung wieder da – "
              << "START blinkt schnell, kurzer START beider Anlagen setzt Betrieb fort."
             );

    if (m_context == nullptr) {
        return;
    }

    level1::applyReadyForActiveEntry(
        m_context->getDispatcher(), m_context->getEventManager(), m_context);

    if (auto* errorHandler = m_context->getErrorHandler()) {
        errorHandler->reconcileChuteWarningOnOperationalEntry();
    }
}

void ReadyForActiveState::exit() {
    if (m_context == nullptr) {
        return;
    }

    level1::applyReadyForActiveExit(m_context->getDispatcher(), m_context->getEventManager());
}

void ReadyForActiveState::handle(Event e) {
    if (m_context == nullptr) {
        return;
    }

    switch (e) {
    case Event::START_SHORT_PRESSED:
        if (!level1::canEnterRunning(m_context->getEventManager())) {
            std::cout << "[ReadyForActive] START ignoriert – Partner/Kalibrierung nicht bereit."
                      << std::endl;
            break;
        }
        DETAIL_LOG( "[ReadyForActive] Kurzer START -> Running fortsetzen (beide Anlagen)"
                 );
        enterRunningResumed(m_context);
        break;

    case Event::PARTNER_START_RUNNING:
        if (!level1::canEnterRunning(m_context->getEventManager())) {
            std::cout << "[ReadyForActive] Partner-START ignoriert – lokal nicht bereit."
                      << std::endl;
            break;
        }
        DETAIL_LOG( "[ReadyForActive] Partner START_RUNNING -> Running fortsetzen");
        m_context->transitionTo(new RunningState());
        break;

    case Event::START_LONG_PRESSED:
        DETAIL_LOG( "[ReadyForActive] Langer START im pausierten Betrieb ignoriert."
                 );
        break;

    case Event::STOP_RELEASED:
        DETAIL_LOG( "[ReadyForActive] STOP -> IdleState");
        m_context->transitionTo(new IdleState());
        break;

    case Event::PARTNER_CALIB_NOT_READY:
    case Event::DATA_NOT_EXIST:
    case Event::CONNECTION_LOST:
        DETAIL_LOG( "[ReadyForActive] Bedingungen nicht mehr erfuellt -> IdleState");
        m_context->transitionTo(new IdleState());
        break;

    case Event::DATA_EXIST:
    case Event::PARTNER_CALIB_READY:
        break;

    default:
        break;
    }
}

} // namespace logik
