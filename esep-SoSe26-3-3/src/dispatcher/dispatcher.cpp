/**
 * @file dispatcher.cpp
 * @brief Implementierung des zentralen Nachrichtenverteilers.
 */

#include "dispatcher.hpp"

#include <iostream>
#include <cerrno>
#include <cstring>
#include <sys/neutrino.h>

// Konstruktor: Erstellt den Briefkasten
Dispatcher::Dispatcher(/* std::shared_ptr<SortierLogik> logik */)
// : m_logik(logik) // (Später einkommentieren, wenn die Logik da ist)
{
    // 1. QNX Channel erstellen. Die '0' bedeutet Standard-Flags.
    m_chid = ChannelCreate(0);

    if (m_chid == -1) {
        std::cerr << "KRITISCHER FEHLER: Dispatcher konnte QNX Channel nicht erstellen! "
                  << strerror(errno) << std::endl;
        exit(EXIT_FAILURE); // Ohne Channel kann die Anlage nicht funktionieren!
    }

    std::cout << "[Dispatcher] QNX Channel erfolgreich erstellt. CHID: " << m_chid << std::endl;
}

// Destruktor: Räumt hinter sich auf
Dispatcher::~Dispatcher() {
    if (m_chid != -1) {
        ChannelDestroy(m_chid);
        std::cout << "[Dispatcher] QNX Channel zerstoert." << std::endl;
    }
}

int Dispatcher::getChannelId() const {
    return m_chid;
}

// Die Endlosschleife (Der Reaktor)
void Dispatcher::run() {
    std::cout << "[Dispatcher] Starte Event-Loop. Warte auf Hardware-Pulses..." << std::endl;

    struct _pulse pulse;

    while (true) {
        // 1. Blockieren und auf den nächsten Pulse warten
        // Der Thread schläft hier, bis jemand MsgSendPulse() aufruft!
        int rcvid = MsgReceivePulse(m_chid, &pulse, sizeof(pulse), nullptr);

        if (rcvid == -1) {
            std::cerr << "[Dispatcher] Fehler beim Empfangen des Pulses: "
                      << strerror(errno) << std::endl;
            continue; // Versuche es bei einem Fehler einfach nochmal
        }

        // 2. Pulse auswerten (Wer ist der Absender?)
        switch (pulse.code) {

            case ipc::HAL_INPUT:
                // Ein Interrupt von der Hardware (ADC, Lichtschranke, Tasten) ist da!
                // Die Nutzdaten (z.B. HEIGHT_ADC + WP_TALL) stecken in pulse.value.sival_int
                handleHalPulse(pulse.value.sival_int);
                break;

            // Hier kommt später euer Timer-Event Code hin
            // case ipc::TIMER_EVENT:
            //     handleTimerPulse(pulse.value.sival_int);
            //     break;

            case _PULSE_CODE_DISCONNECT:
                // Ein QNX-interner Code: Jemand hat die Verbindung (coid) zu uns geschlossen
                std::cout << "[Dispatcher] Ein Client hat die Verbindung getrennt." << std::endl;
                break;

            default:
                std::cerr << "[Dispatcher] Unbekannter Pulse-Code empfangen: "
                          << pulse.code << std::endl;
                break;
        }
    }
}


void Dispatcher::handleHalPulse(uint32_t payload) {
    // 1. Auspacken
    uint16_t sensor_id = ipc::decodeSensorID(payload);
    uint16_t sensor_state = ipc::decodeSensorState(payload);

    // Diese Konsolenausgabe hilft dir zu sehen, welche ID welche Taste hat
    std::cout << "[Dispatcher] Event -> Sensor: " << sensor_id << " | Zustand: " << sensor_state << std::endl;

    // Sicherheitsabfrage: Wurde die Aktorik übergeben?
    if (!m_actuators) return;

    // ========================================================================
    // TASTE 1: START -> Grüne Ampel blinkt schnell
    // ========================================================================
    if (sensor_id == ipc::BTN_START) {
        if (sensor_state == 1) {
            std::cout << ">>> TEST: Start gedrueckt! Gruen blinkt! <<<" << std::endl;
            m_actuators->lightGreenBlinkFast();
        }
        else {
            std::cout << ">>> TEST: Start losgelassen! Gruen aus! <<<" << std::endl;
            m_actuators->lightGreenOff();
        }
    }
    // ========================================================================
    // TASTE 2: RESET -> Gelbe Ampel blinkt schnell
    // ========================================================================
    else if (sensor_id == ipc::BTN_RESET) {
        if (sensor_state == 1) {
            std::cout << ">>> TEST: Reset gedrueckt! Gelb blinkt! <<<" << std::endl;
            m_actuators->lightYellowBlinkFast();
        }
        else {
            std::cout << ">>> TEST: Reset losgelassen! Gelb aus! <<<" << std::endl;
            m_actuators->lightYellowOff();
        }
    }
    // ========================================================================
    // TASTE 3: STOP -> Rote Ampel blinkt schnell
    // ========================================================================
    else if (sensor_id == ipc::BTN_STOP) {
        if (sensor_state == 0) {
            // ACHTUNG: Der Stop-Taster an der Festo-Anlage ist oft "Active Low" (Öffner)!
            // Wenn 0 = "gedrückt" bedeutet, musst du das hier anpassen.
            // Falls dein System 1 für "gedrückt" schickt, mach aus der 0 eine 1.
            std::cout << ">>> TEST: Stop gedrueckt! Rot blinkt! <<<" << std::endl;
            m_actuators->lightRedBlinkSlow();
        }
        else {
            std::cout << ">>> TEST: Stop losgelassen! Rot aus! <<<" << std::endl;
            m_actuators->lightRedOff();
        }
    }

    // ========================================================================
    // LICHTSCHRANKE 1: Einlauf (Entry)
    // ========================================================================
    else if (sensor_id == ipc::LS_ENTRY) { // Namen evtl. an deine ipc_definitions.hpp anpassen!
        // Zustand 0 = Lichtschranke wurde durch ein Werkstück unterbrochen
        if (sensor_state == 0) {
            std::cout << ">>> TEST: Werkstueck am Einlauf erkannt! Motor faehrt RECHTS! <<<" << std::endl;
            m_actuators->driveRight();
            m_actuators->lightGreenBlinkSlow(); // Ein bisschen Show dazu machen
        }
    }

    // ========================================================================
    // LICHTSCHRANKEN 2 & 3: Auslauf (End) oder Rutsche (Chute)
    // ========================================================================
    else if (sensor_id == ipc::LS_END) {
        if (sensor_state == 0) {
            std::cout << ">>> TEST: Werkstueck am Ende angekommen! Motor STOPP! <<<" << std::endl;
            m_actuators->driveStop();
            m_actuators->lightGreenOff();
        }
    }
    else if (sensor_id == ipc::LS_CHUTE) {
        if (sensor_state == 0) {
            std::cout << ">>> TEST: Werkstueck in Rutsche gefallen! Motor STOPP! <<<" << std::endl;
            m_actuators->driveStop();
            m_actuators->lightGreenOff();
        }
    }
}




// Private Verteiler-Methode für Timer-Events (Vorbereitet für später)
void Dispatcher::handleTimerPulse(int timer_id) {
    std::cout << "[Dispatcher] TIMER abgelaufen! ID: " << timer_id << std::endl;
    // m_logik->verarbeiteTimeout(timer_id);
}
