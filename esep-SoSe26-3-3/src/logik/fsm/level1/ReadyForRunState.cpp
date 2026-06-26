#include "ReadyForRunState.hpp"
#include "IdleState.hpp"
#include "RunningState.hpp"
#include "ServiceState.hpp"
#include "logik/logik.hpp"
#include "logik/fsm/level1/Level1Actions.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include "shared/detailLog.hpp"
#include <iostream>

namespace logik {

namespace {

void enterRunningWithPartner(Logik* context) {
    if (context == nullptr) {
        return;
    }

    if (auto* dispatcher = context->getDispatcher()) {
        dispatcher->sendPartnerStartRunning();
    }

    context->transitionTo(new RunningState());
}

void enterServiceWithPartner(Logik* context) {
    if (context == nullptr) {
        return;
    }

    if (auto* dispatcher = context->getDispatcher()) {
        dispatcher->sendPartnerStartService();
        dispatcher->sendPartnerCalibStatus(false);
    }

    context->transitionTo(new ServiceState());
}

} // namespace

void ReadyForRunState::entry() {
    DETAIL_LOG( "[ReadyForRun] Entered (Ebene 1). Beide Anlagen bereit – START an, kurzer START fuer Running."
             );

    if (m_context == nullptr) {
        return;
    }

    level1::applyReadyForRunEntry(
        m_context->getDispatcher(), m_context->getEventManager(), m_context);
}

void ReadyForRunState::exit() {
    if (m_context == nullptr) {
        return;
    }

    level1::applyReadyForRunExit(m_context->getDispatcher(), m_context->getEventManager());
}

void ReadyForRunState::handle(Event e) {
    if (!m_context) {
        return;
    }

    switch (e) {
    case Event::START_SHORT_PRESSED:
        DETAIL_LOG( "[ReadyForRun] Kurzer START -> Running (beide Anlagen)");
        enterRunningWithPartner(m_context);
        break;

    case Event::START_LONG_PRESSED:
        DETAIL_LOG( "[ReadyForRun] Langer START -> Service (Partner wartet in Idle)");
        enterServiceWithPartner(m_context);
        break;

    case Event::PARTNER_START_RUNNING:
        DETAIL_LOG( "[ReadyForRun] Partner START_RUNNING -> RunningState");
        m_context->transitionTo(new RunningState());
        break;

    case Event::PARTNER_START_SERVICE:
        DETAIL_LOG( "[ReadyForRun] Partner startet Service -> Idle, Partner-Kalibrierung NICHT bereit"
                 );
        if (auto* eventManager = m_context->getEventManager()) {
            eventManager->setPartnerCalibrationReady(false);
        }
        m_context->transitionTo(new IdleState());
        break;

    case Event::STOP_RELEASED:
        DETAIL_LOG( "[ReadyForRun] STOP -> IdleState");
        m_context->transitionTo(new IdleState());
        break;

    case Event::PARTNER_CALIB_NOT_READY:
    case Event::DATA_NOT_EXIST:
    case Event::CONNECTION_LOST:
        DETAIL_LOG( "[ReadyForRun] Bedingungen nicht mehr erfuellt -> IdleState");
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
