#include "ReadyForServiceState.hpp"
#include "IdleState.hpp"
#include "ServiceState.hpp"
#include "logik/logik.hpp"
#include "logik/fsm/level1/Level1Actions.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void ReadyForServiceState::entry() {
    std::cout << "[ReadyForService] Entered (Ebene 1). Keine Kalibrierung – START blinkt, "
              << "langer START (>=1.5s) fuer Service." << std::endl;

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
        std::cout << "[ReadyForService] START gedrueckt – halten fuer Service..." << std::endl;
        break;

    case Event::START_SHORT_PRESSED:
        std::cout << "[ReadyForService] START zu kurz – bitte mind. 1.5 Sekunden halten."
                  << std::endl;
        if (dispatcher != nullptr) {
            dispatcher->turnLEDStartBlinkSlow();
        }
        break;

    case Event::START_LONG_PRESSED:
        std::cout << "[ReadyForService] Langer START -> ServiceState" << std::endl;
        m_context->transitionTo(new ServiceState());
        break;

    case Event::DATA_EXIST:
        std::cout << "[ReadyForService] Kalibrierungsdatei vorhanden -> IdleState" << std::endl;
        m_context->transitionTo(new IdleState());
        break;

    case Event::STOP_RELEASED:
        std::cout << "[ReadyForService] STOP -> IdleState" << std::endl;
        m_context->transitionTo(new IdleState());
        break;

    default:
        break;
    }
}

} // namespace logik
