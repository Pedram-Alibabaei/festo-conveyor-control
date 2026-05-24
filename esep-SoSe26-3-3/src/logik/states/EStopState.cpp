#include "EStopState.hpp"
#include "IdleState.hpp"
#include <iostream>

namespace logik {

void EStopState::onEnter() {
  std::cout << "[EStopState] Entered. Sicherer Zustand eingenommen!"
            << std::endl;
  m_isHardwareUnlocked = false;
  if (m_context && m_context->getDispatcher()) {
    m_context->getDispatcher()->driveStop();
    m_context->getDispatcher()->turnAllOff();
    m_context->getDispatcher()->lightRedBlinkFast(); // UC 004: schnelles Blinken von LIGHT_RED
  }
}

void EStopState::handle(Event e) {
  if (!m_context || !m_context->getDispatcher())
    return;

  switch (e) {
  case Event::ESTOP_RELEASED:
  case Event::NET_ESTOP_RELEASED:
    std::cout
        << "[EStopState] ESTOP_RELEASED -> Hardware entriegelt, warte auf RESET"
        << std::endl;
    m_isHardwareUnlocked = true;
    m_context->getDispatcher()->turnLEDQ2Off();
    m_context->getDispatcher()->lightRedOn(); // Zustand: entriegelt, aber noch nicht resettet
    break;
  case Event::RESET_PRESSED:
    if (m_isHardwareUnlocked) {
      std::cout << "[EStopState] RESET_PRESSED -> Erfolgreicher Neustart!"
                << std::endl;
      m_context->getDispatcher()->lightRedOff();
      m_context->transitionTo(new IdleState());
    } else {
      std::cout
          << "[EStopState] RESET_PRESSED -> Blockiert, E-Stop noch gedrueckt!"
          << std::endl;
    }
    break;
  default:
    // Im EStopState werden alle anderen Events ignoriert
    break;
  }
}

} // namespace logik
