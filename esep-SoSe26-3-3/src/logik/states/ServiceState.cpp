#include "ServiceState.hpp"
#include "EStopState.hpp"
#include "IdleState.hpp"
#include "dispatcher/dispatcher.hpp" // WICHTIG: Dispatcher Include hinzugefuegt!
#include <iostream>

namespace logik {

void ServiceState::onEnter() {
  std::cout << "[ServiceState] Entered. Kalibrierungen koennen starten!"
            << std::endl;
}

void ServiceState::handle(Event e) {
  if (!m_context || !m_context->getDispatcher())
    return;

  switch (e) {
  case Event::ESTOP_PRESSED:
          std::cout << "[ServiceState] LOKALER ESTOP -> Wechsel zu EStopLocalActiveState" << std::endl;
          m_context->transitionTo(new EStopLocalActiveState());
          break;

      case Event::PARTNER_ESTOP_PRESSED:
          std::cout << "[ServiceState] PARTNER ESTOP -> Wechsel zu EStopPartnerActiveState" << std::endl;
          m_context->transitionTo(new EStopPartnerActiveState());
          break;
  case Event::STOP_PRESSED:
    std::cout << "[ServiceState] STOP_PRESSED -> Zurueck in Ruhezustand"
              << std::endl;
    m_context->getDispatcher()->lightGreenOff();
    m_context->getDispatcher()->lightRedBlinkSlow();
    m_context->transitionTo(new IdleState());
    break;
  default:
    break;
  }
}

} // namespace logik
