/**
 * @file Logik.cpp
 * @brief Implementierung des zentralen Sch Schicht-1-Controllers (Zustandsmaschine).
 *
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#include "Logik.hpp"
#include <iostream>

namespace logik {

Logik::Logik(i_Dispatcher* dispatcher)
    : m_wpManager(std::make_unique<WorkpieceManager>(this)),
      m_dispatcher(dispatcher),
      m_anlagenPosition(AnlagenPosition::UNKNOWN_POSITION),
      m_isPartnerChuteFull(false)
{
    std::cout << "[Logik] System-Instanz erfolgreich konstruiert." << std::endl;
}

void Logik::init(AnlagenPosition pos) {
    // 1. Speichere die physische Position lokal in der Hauptsteuerung
    m_anlagenPosition = pos;

    // 2. REICHE DIE POSITION AN DEN WORKPIECE-MANAGER WEITER
    m_wpManager->setAnlagenPosition(pos);

    // Ausführliche Konsolenausgabe für das Labor-Debugging beim Systemstart
    if (pos == AnlagenPosition::VORN_ANLAGE_1) {
        std::cout << "[Logik-Init] Instanz erfolgreich als ANLAGE 1 (VORN / MASTER) initialisiert." << std::endl;
    } else if (pos == AnlagenPosition::HINTEN_ANLAGE_2) {
        std::cout << "[Logik-Init] Instanz erfolgreich als ANLAGE 2 (HINTEN / SLAVE) initialisiert." << std::endl;
    } else {
        std::cerr << "[Logik-Init] CRITICAL: Unbekannte oder ungueltige Anlagenposition empfangen!" << std::endl;
    }
}

void Logik::processEvent(logik::Event e) {
    // Hier befindet sich eure Schicht-1-Zustandsmaschine (Haupt-FSM der Anlage).
    // Diese wertet globale Zustände wie READY, RUNNING, STOP, ERROR, E-STOP aus.
    // Parallel dazu werden die Sensor-Events direkt an das Schieberegister weitergereicht:

    switch (e) {
        // Events, die den globalen Anlagenzustand steuern (Beispiele für eure FSM)
        case logik::Event::SET_STOP:
            std::cout << "[FSM] Wechsel in Zustand: STOP. Stoppe Motoren..." << std::endl;
            m_dispatcher->stopMotor();
            break;

        case logik::Event::ERROR:
            std::cerr << "[FSM-CRITICAL] Wechsel in Zustand: ERROR! Gesamtanlage angehalten. Rote Leuchte aktiv." << std::endl;
            m_dispatcher->stopMotor();
            m_dispatcher->flashRedFast();
            break;

        // Alle hardwarenahen Werkstück-Sensorflanken werden an den Manager delegiert
        case logik::Event::ENTRY_BLOCKED:
            // Da ENTRY_BLOCKED laut Architektur ein neues Objekt erzeugt, nutzen wir die Direktmethode:
            if (m_anlagenPosition == AnlagenPosition::VORN_ANLAGE_1) {
                m_wpManager->addWorkpiece();
            } else {
                std::cerr << "[Logik-Safety] ENTRY_BLOCKED auf Anlage 2 ignoriert! Teile muessen via Netzwerk kommen." << std::endl;
            }
            break;

        case logik::Event::ENTRY_CLEARED:
        case logik::Event::HEIGHT_BLOCKED:
        case logik::Event::HEIGHT_CLEARED:
        case logik::Event::HEIGHT_ERROR:
        case logik::Event::METAL_DETECTED:
        case logik::Event::METAL_CLEARED:
        case logik::Event::EJECTOR_BLOCKED:
        case logik::Event::EJECTOR_CLEARED:
        case logik::Event::END_BLOCKED:
        case logik::Event::END_CLEARED:
        case logik::Event::CHUTE_BLOCKED:
        case logik::Event::CHUTE_CLEARED:
            // Standard-Sensorflanken stur an das Schieberegister durchreichen
            m_wpManager->handleSensorSignal(e);
            break;

        default:
            break;
    }
}

void Logik::workpieceTypeFromHS(WpType type) {
    // Die HAL hat die Analogmessung des Höhensensors abgeschlossen und liefert den Typ.
    // Wir reichen dieses Signal mitsamt dem ermittelten Typ an den Manager weiter.
    std::cout << "[Logik] Typmeldung von Hoehensensor empfangen (Typ-ID: " << static_cast<int>(type) << ")." << std::endl;
    m_wpManager->handleSensorSignal(logik::Event::HEIGHT_CLEARED, type);
}

void Logik::workpieceInfo(int id, WpType type) {
    // Diese Methode wird vom Netzwerk-Framework (Empfängerseite des Dispatchers) aufgerufen,
    // wenn Anlage 1 ein Werkstück über die Kante schiebt.
    if (m_anlagenPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
        std::cout << "[Netzwerk-Empfang] Digitaler Zwilling von Anlage 1 uebermittelt. ID: " << id << std::endl;

        // Gebäre das übertragene Werkstück im Schieberegister von Anlage 2
        m_wpManager->addNetworkWorkpiece(id, type);
    } else {
        std::cerr << "[Logik-Netzwerk] WARNUNG: Anlage 1 hat fälschlicherweise ein workpieceInfo-Event empfangen!" << std::endl;
    }
}

void Logik::updateWpListe(WpType type) {
    // ========================================================================
    // ABSOLUTE REZEPT-SYNCHRONISATION (Deine krisenfeste Fehlerkorrektur)
    // Wenn Anlage 2 eine physische Manipulation erkennt, schickt sie diese
    // Nachricht zurück an Anlage 1. Anlage 1 zwingt ihr Rezept hart zurück.
    // ========================================================================
    if (m_anlagenPosition == AnlagenPosition::VORN_ANLAGE_1) {
        std::cout << "[Netzwerk-Event] Anlage 2 meldet physische Manipulation!" << std::endl;
        std::cout << " -> Zwinge lokales Rezept hart zurueck auf Typ-ID: " << static_cast<int>(type) << std::endl;

        // Greift über den neuen Getter der HPP direkt auf die forceType()-Methode zu
        m_wpManager->getSequence().forceType(type);

        // Da das Gesamtsystem manipuliert wurde, eskalieren wir auch auf Anlage 1 einen Fehler
        this->processEvent(logik::Event::ERROR);
    } else {
        std::cerr << "[Logik-Netzwerk] Fehler: updateWpListe auf Anlage 2 aufgerufen!" << std::endl;
    }
}

void Logik::takt() {
    // Wird stur alle 10ms vom Hardware-Timer-Thread aufgerufen.
    // Wir ermitteln den aktuellen Motorstatus aus eurer FSM (hier beispielhaft simuliert)
    // und reichen den Takt an den Manager weiter, damit die Transit-Teile altern können.
    bool isMotorSlow = false; // Sollte real aus eurem globalen FSM-Zustand gelesen werden

    m_wpManager->takt(isMotorSlow);
}

void Logik::setPartnerChuteStatus(bool isFull) {
    m_isPartnerChuteFull = isFull;

    // Wenn wir Anlage 1 sind und die Rutsche von Anlage 2 voll ist, müssen wir
    // das dem WorkpieceManager mitteilen, damit die Übergabe-Sperre greift
    if (m_anlagenPosition == AnlagenPosition::VORN_ANLAGE_1) {
        m_wpManager->setAnlage2Ready(!isFull);
    }
}

void Logik::setCalibrationData(const CalibrationData& data) {
    std::cout << "[Logik] Vollstaendiges Kalibrierungspaket aus Konfigurationsdatei empfangen." << std::endl;
    std::cout << " -> Reiche Paket geschlossen an den WorkpieceManager weiter..." << std::endl;

    // Direktes Durchreichen des Structs ohne mühsame Einzelaufrufe
    m_wpManager->setCalibrationData(data);
}

} // namespace logik