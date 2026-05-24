//
// Created by bahoz on 23.05.2026.
//

/**
 * @file Logik.cpp
 * @brief Implementierung des zentralen Logik-Herzens und Zustandsautomaten.
 */

#include "Logik.hpp"
#include <iostream>
#include "shared/logikEvent.hpp"

void Logik::init(AnlagenPosition pos) {
    m_position = pos;
    std::string posName = (pos == AnlagenPosition::VORN_ANLAGE_1) ? "Anlage 1 (Vorn)" : "Anlage 2 (Hinten)";
    std::cout << "[Logik] Initialisiert fuer Position: " << posName << std::endl;

    // Hier spaeter: Instanzen der FSMs oder des WorkpieceManagers aufsetzen
}

void Logik::processEvent(logik::Event e) {
    // Hier laeuft spaeter jedes Sensor- und Taster-Signal auf!
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
            // Temporäres generisches Logging für alle noch nicht explizit behandelten Events
            // (Verhindert, dass die Konsole bei Ticks überflutet wird)
            break;
    }
}

void Logik::workpieceTypeFromHS(WpType type) {
    std::cout << "[Logik-ADC] Profiler meldet erkannten Typ: " << static_cast<int>(type) << std::endl;
    // Hier spaeter: Dem aktuell vermessenen Werkstueck in der Liste seinen Typ zuweisen
}

void Logik::workpieceInfo(int id, WpType type) {
    std::cout << "[Logik-Netzwerk] Info von Partner: ID " << id
              << " vom Typ " << static_cast<int>(type) << " kommt zu uns." << std::endl;
    // Hier spaeter: Werkstueck in die Empfangs-Liste der Anlage 2 eintragen
}

void Logik::updateWpListe(WpType type) {
    std::cout << "[Logik-Netzwerk] Partner bittet um Listen-Update fuer Typ " << static_cast<int>(type) << std::endl;
}

void Logik::tackt() {
    // Diese Methode wird bei JEDEM Belt-Tick (z.B. alle 10ms) aufgerufen.
    // Extrem wichtig fuer das Schieberegister / die Positionsverfolgung!

    // Hier spaeter: m_workpieceManager.updatePositions();
}
