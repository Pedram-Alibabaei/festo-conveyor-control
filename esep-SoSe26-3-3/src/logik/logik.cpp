/**
 * @file logik.cpp
 * @brief Implementierung des Contextes (GoF State Pattern) für die Festo-Anlage.
 */

#include "Logik.hpp"
#include "dispatcher/I_Dispatcher.hpp" // Header für Dispatcher-Aufrufe binden
#include <iostream>
#include "shared/logikEvent.hpp"

// ============================================================================
// KONSTRUKTOR & INITIALISIERUNG
// ============================================================================

Logik::Logik()
    : m_position(AnlagenPosition::VORN_ANLAGE_1),
      m_countTicks(false),
      m_tickCounter(0),
      m_dispatcher(nullptr),
      m_isCalibrated(false) { // Am Start ist die Anlage noch unkalibriert
}

Logik::~Logik() {
    // Sicherer, leerer virtueller Destruktor zur Behebung des vtable-Fehlers
}

void Logik::init(AnlagenPosition pos) {
    m_position = pos;
    std::string posName = (pos == AnlagenPosition::VORN_ANLAGE_1) ? "Anlage 1" : "Anlage 2";
    std::cout << "[Logik] Initialisiert fuer Position: " << posName << std::endl;
}

// ============================================================================
// CENTRAL EVENT PROCESSING (Zentrale Ereignis-Verarbeitung)
// ============================================================================

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
    // Hier liest die Logik das Ergebnis deiner 'evaluationFinished'-Methode aus!
    std::string name = "UNKNOWN";
    switch (type) {
        case WpType::FLAT:         name = "FLAT (Flach)"; break;
        case WpType::TALL:         name = "TALL (Hoch)"; break;
        case WpType::TALL_DRILLED: name = "TALL_DRILLED (Gebohrt)"; break;
        default:                   name = "UNBEKANNT / FEHLER"; break;
    }

    std::cout << "\n==================================================" << std::endl;
    std::cout << " [LOGIK-AUSWERTUNG] Werkstueck-Typ ist: " << name << std::endl;
    std::cout << "==================================================\n" << std::endl;
void Logik::tackt() {
    // Diese Methode wird bei JEDEM Belt-Tick aufgerufen.
}

void Logik::setTrackedSegmentTicks(uint16_t calibMode, uint16_t tickValue) {
    std::cout << "[Logik-Kalib] Erhaltene Wegstrecken-Ticks: Segment-Modus="
              << calibMode << " Ticks=" << tickValue << std::endl;

    // Hier fütterst du später die Timings deiner FSM/ deines Schieberegisters,
    // damit die Weiche exakt zum richtigen Takt-Zeitpunkt öffnet!
}

void Logik::receiveRawCalibrationData(uint16_t calibMode, uint16_t adcValue) {
    // Wird während des Live-Einlernens im Service-Modus getriggert
    std::cout << "[Logik-Kalib] Live-ADC-Kalibrierwert empfangen: Modus="
              << calibMode << " Wert=" << adcValue << std::endl;
}

} // namespace logik