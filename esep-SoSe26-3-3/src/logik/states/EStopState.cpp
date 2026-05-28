#include "EStopState.hpp"
#include "IdleState.hpp"
#include "logik/logik.hpp"
#include "dispatcher/dispatcher.hpp"
#include "logik/i_Logik.hpp"
#include <iostream>

namespace logik {

void EStopState::onEnter() {
    std::cout << "[EStopState] Entered. Sicherer Zustand eingenommen!" << std::endl;
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
    // KORRIGIERT: Event-Namen an die Struktur des Dispatchers angepasst
    case Event::ESTOP_RELEASED:
    case Event::PARTNER_ESTOP_RELEASED:
        std::cout << "[EStopState] E-Stop entriegelt -> Warte auf RESET-Quittierung." << std::endl;
        m_isHardwareUnlocked = true;
        m_context->getDispatcher()->turnLEDQ2Off();
        m_context->getDispatcher()->lightRedOn(); // Zustand: entriegelt, aber noch nicht quittiert
        break;

    // ERWEITERUNG: Falls im entriegelten Zustand erneut ein E-Stop gedrueckt wird
    case Event::ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_PRESSED:
        std::cout << "[EStopState] Warnung: E-Stop erneut verriegelt!" << std::endl;
        m_isHardwareUnlocked = false;
        m_context->getDispatcher()->lightRedBlinkFast();
        break;

    case Event::RESET_PRESSED:
        if (m_isHardwareUnlocked) {
            std::cout << "[EStopState] RESET_PRESSED -> Duale Quittierung erfolgreich. Wechsle in IdleState." << std::endl;
            m_context->getDispatcher()->lightRedOff();
            m_context->transitionTo(new IdleState());
        } else {
            std::cout << "[EStopState] RESET_PRESSED blockiert: Mindestens ein E-Stop ist noch aktiv!" << std::endl;
        }
        break;

    default:
        // Im EStopState werden alle prozessrelevanten Sensor-Events (z.B. LS_ENTRY) blockiert
        break;
    }
}

} // namespace logik
