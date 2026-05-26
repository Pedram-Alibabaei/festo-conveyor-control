/**
 * @file Logik.cpp
 * @brief Implementierung des zentralen Logik-Herzens und Zustandsautomaten.
 */

#include "Logik.hpp"
#include <iostream>
#include "shared/logikEvent.hpp"

namespace logik { // Namespace-Block geöffnet

void Logik::init(AnlagenPosition pos) {
    m_position = pos;
    std::string posName = (pos == AnlagenPosition::VORN_ANLAGE_1) ? "Anlage 1 (Vorn)" : "Anlage 2 (Hinten)";
    std::cout << "[Logik] Initialisiert fuer Position: " << posName << std::endl;

    // Hier später: Instanzen der FSMs oder des WorkpieceManagers aufsetzen
}

void Logik::processEvent(logik::Event e) {
    switch (e) {
        case logik::Event::ENTRY_BLOCKED:
            std::cout << "[Logik-FSM] Werkstueck am Einlauf erkannt." << std::endl;
            break;
        case logik::Event::ENTRY_CLEARED:
            std::cout << "[Logik-FSM] Werkstueck hat Einlauf verlassen." << std::endl;
            break;
        case logik::Event::HEIGHT_BLOCKED:
            std::cout << "[Logik-FSM] Werkstueck hat Hoehenmessung erreicht." << std::endl;
            break;
        case logik::Event::HEIGHT_CLEARED:
            std::cout << "[Logik-FSM] Hoehenmessung beendet." << std::endl;
            break;

        // Buttons
        case logik::Event::START_PRESSED:
            std::cout << "[Logik-FSM] Start-Taster gedrueckt -> Starte Anlage." << std::endl;
            break;
        case logik::Event::STOP_PRESSED:
            std::cout << "[Logik-FSM] Stop-Taster gedrueckt -> Stoppe Anlage." << std::endl;
            break;
        case logik::Event::ESTOP_PRESSED:
            std::cerr << "[Logik-FSM] !!! NOT-HALT AKTIVIERT !!!" << std::endl;
            break;
        case logik::Event::ESTOP_RELEASED:
            std::cout << "[Logik-FSM] Not-Halt entriegelt. Warte auf Reset." << std::endl;
            break;

        default:
            break;
    }
}

void Logik::workpieceTypeFromHS(WpType type) {
    std::cout << "[Logik-ADC] Profiler meldet erkannten Typ: " << static_cast<int>(type) << std::endl;
    // Hier später: Dem aktuell vermessenen Werkstueck in der Liste seinen Typ zuweisen
}

void Logik::workpieceInfo(int id, WpType type) {
    std::cout << "[Logik-Netzwerk] Info von Partner: ID " << id
              << " vom Typ " << static_cast<int>(type) << " kommt zu uns." << std::endl;
}

void Logik::updateWpListe(WpType type) {
    std::cout << "[Logik-Netzwerk] Partner bittet um Listen-Update fuer Typ " << static_cast<int>(type) << std::endl;
}

void Logik::tackt() {
    // Diese Methode wird bei JEDEM Belt-Tick (z.B. alle 10ms) aufgerufen.
    // Hier später: m_workpieceManager.updatePositions();
}

// ============================================================================
// KALIBRIERUNGS-SCHNITTSTELLEN (NEU)
// ============================================================================

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