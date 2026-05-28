/**
 * @file logik.cpp
 * @brief Implementierung des Contextes (GoF State Pattern) für die Festo-Anlage.
 */

#include "logik.hpp"
#include "states/BaseState.hpp"
#include "states/IdleState.hpp" // Startzustand
#include <iostream>

namespace logik {

Logik::Logik() : m_position(UNKNOWN_POSITION), m_currentState(nullptr), m_dispatcher(nullptr) {
}

Logik::~Logik() {
    if (m_currentState) {
        delete m_currentState;
    }
}

void Logik::init(AnlagenPosition pos) {
    m_position = pos;
    std::string posName = (pos == AnlagenPosition::VORN_ANLAGE_1) ? "Anlage 1" : "Anlage 2";
    std::cout << "[Logik] Initialisiert fuer Position: " << posName << std::endl;

    // 1. Startzustand festlegen
    transitionTo(new IdleState());
}

// ============================================================================
// STATE PATTERN (GoF) CORE
// ============================================================================

void Logik::transitionTo(BaseState* newState) {
    // 1. Wenn wir schon in einem Zustand sind, dessen onExit() aufrufen und loeschen
    if (m_currentState != nullptr) {
        m_currentState->onExit();
        delete m_currentState;
    }

    // 2. Neuen Zustand setzen und mit diesem Kontext verknuepfen
    m_currentState = newState;
    m_currentState->setContext(this);

    // 3. Start-Routine des neuen Zustands aufrufen
    m_currentState->onEnter();
}

void Logik::processEvent(logik::Event e) {
    // DAS IST DAS HERZSTÜCK DES PATTERNS:
    // Kein switch-case mehr! Wir delegieren blind an den aktuellen Zustand.
    if (m_currentState) {
        m_currentState->handle(e);
    } else {
        std::cerr << "[Logik] FATAL: Kein aktiver Zustand gesetzt!" << std::endl;
    }
}

// ============================================================================
// WERKSTUECK-TRACKING (Dummys fuer den Compiler)
// ============================================================================

void Logik::addWorkpiece() {
    std::cout << "[Logik] addWorkpiece() aufgerufen. (Tracking noch nicht implementiert)" << std::endl;
}

bool Logik::dispatchSensorEventToWorkpiece(logik::Event e) {
    std::cout << "[Logik] dispatchSensorEventToWorkpiece() (Tracking noch nicht implementiert)" << std::endl;
    return true;
}

// ============================================================================
// INTERFACE IMPLEMENTIERUNG (Rest)
// ============================================================================

void Logik::workpieceTypeFromHS(WpType type) {
    std::cout << "[Logik-ADC] Profiler meldet erkannten Typ: " << static_cast<int>(type) << std::endl;
}

void Logik::workpieceInfo(int id, WpType type) {
    std::cout << "[Logik-Netzwerk] Info von Partner: ID " << id
              << " vom Typ " << static_cast<int>(type) << " kommt zu uns." << std::endl;
}

void Logik::updateWpListe(WpType type) {
    std::cout << "[Logik-Netzwerk] Partner bittet um Listen-Update fuer Typ " << static_cast<int>(type) << std::endl;
}

void Logik::tackt() {
    // Diese Methode wird bei JEDEM Belt-Tick aufgerufen.
}

void Logik::setTrackedSegmentTicks(uint16_t calibMode, uint16_t tickValue) {
    std::cout << "[Logik-Kalib] Erhaltene Wegstrecken-Ticks: Segment-Modus="
              << calibMode << " Ticks=" << tickValue << std::endl;
}

void Logik::receiveRawCalibrationData(uint16_t calibMode, uint16_t adcValue) {
    std::cout << "[Logik-Kalib] Live-ADC-Kalibrierwert empfangen: Modus="
              << calibMode << " Wert=" << adcValue << std::endl;
}

} // namespace logik
