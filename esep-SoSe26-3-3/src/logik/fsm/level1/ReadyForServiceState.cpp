#include "ReadyForServiceState.hpp"
#include "IdleState.hpp"
#include "ServiceState.hpp"
#include "logik/logik.hpp"
#include "logik/fsm/level1/Level1Actions.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include "shared/detailLog.hpp"
#include <iostream>

namespace logik {

void ReadyForServiceState::entry() {
    DETAIL_LOG( "[ReadyForService] Entered (Ebene 1). Keine Kalibrierung – START blinkt, "
              << "langer START (>=1.5s) fuer Service.");

    if (m_context == nullptr) {
        return;
    }

    level1::applyReadyForServiceEntry(m_context->getDispatcher(), m_context->getEventManager());
}

void ReadyForServiceState::exit() {
    if (m_context == nullptr) {
        return;
    }

    level1::applyReadyForServiceExit(m_context->getDispatcher(), m_context->getEventManager());
}

void ReadyForServiceState::handle(Event e) {
    if (!m_context) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();

    switch (e) {
    case Event::START_PRESSED:
        if (dispatcher != nullptr) {
            dispatcher->turnLEDStartOn();
        }
        DETAIL_LOG( "[ReadyForService] START gedrueckt – halten fuer Service...");
        break;

    case Event::START_SHORT_PRESSED:
        DETAIL_LOG( "[ReadyForService] START zu kurz – bitte mind. 1.5 Sekunden halten."
                 );
        if (dispatcher != nullptr) {
            dispatcher->turnLEDStartBlinkSlow();
        }
        break;

    case Event::START_LONG_PRESSED:
        DETAIL_LOG( "[ReadyForService] Langer START -> ServiceState");
        m_context->transitionTo(new ServiceState());
        break;

    case Event::DATA_EXIST:
        DETAIL_LOG( "[ReadyForService] Kalibrierungsdatei vorhanden -> IdleState");
        m_context->transitionTo(new IdleState());
        break;

    case Event::STOP_RELEASED:
        DETAIL_LOG( "[ReadyForService] STOP -> IdleState");
        m_context->transitionTo(new IdleState());
        break;

    default:
        break;
    }
}

} // namespace logik
