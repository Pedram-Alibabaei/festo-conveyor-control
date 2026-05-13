/**
 * @file dispatcher.cpp
 * @brief Implementierung des zentralen Nachrichtenverteilers.
 */

#include "dispatcher.hpp"

#include <iostream>
#include <cerrno>
#include <cstring>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

// ============================================================================
// KONSTRUKTOR & DESTRUKTOR
// ============================================================================
Dispatcher::Dispatcher()
    : m_chid(-1),
      m_localCoid(-1),
      m_partnerCoid(-1),
      m_running(false),
      m_actuators(nullptr),
      m_connection(nullptr) {
}

Dispatcher::~Dispatcher() {
    stop();

    // Ressourcen sauber freigeben
    if (m_localCoid != -1) ConnectDetach(m_localCoid);
    if (m_partnerCoid != -1) ConnectDetach(m_partnerCoid);
    if (m_chid != -1) ChannelDestroy(m_chid);
}

// ============================================================================
// Initialisierung
// ============================================================================

void Dispatcher::init(ipc::AnlagenPosition pos) {
    m_position = pos;

    // 1. Kanal erstellen (Das "Ohr" des Dispatchers)
    m_chid = ChannelCreate(0);
    if (m_chid == -1) {
        std::cerr << "[Dispatcher] Fehler: ChannelCreate fehlgeschlagen!" << std::endl;
        return;
    }

    // 2. Verbindung zu sich selbst aufbauen (Die eigene "Telefonnummer")
    m_localCoid = ConnectAttach(0, 0, m_chid, _NTO_SIDE_CHANNEL, 0);
    if (m_localCoid == -1) {
        std::cerr << "[Dispatcher] Fehler: ConnectAttach zu sich selbst fehlgeschlagen!" << std::endl;
        return;
    }

    std::cout << "[Dispatcher] Bereit auf Kanal " << m_chid
              << " (Lokale COID: " << m_localCoid << ")" << std::endl;
}

// ============================================================================
// Hauptschleife (Der Reaktor)
// ============================================================================

void Dispatcher::run() {
    struct _pulse pulse;
    m_running = true;

    std::cout << "[Dispatcher] Reaktor-Schleife gestartet..." << std::endl;

    while (m_running) {
        // BLOCKIERT, bis ein Puls eintrifft
        int rcvid = MsgReceivePulse(m_chid, &pulse, sizeof(pulse), NULL);

        if (rcvid == 0) { // Wir haben einen Puls empfangen
            handlePulse(pulse);
        } else if (rcvid == -1) {
            if (errno == EINTR) continue; // Unterbrochen durch Systemsignal
            std::cerr << "[Dispatcher] Fehler bei MsgReceivePulse!" << std::endl;
            break;
        }
    }
}

// ============================================================================
// Zentraler Event-Verteiler
// ============================================================================

void Dispatcher::handlePulse(const struct _pulse& pulse) {
    switch (pulse.code) {

        case ipc::HAL_INPUT:
            handleHalPulse(pulse.value.sival_int);
            break;

        case ipc::NETWORK_MSG:
            // Der Partner hat uns etwas geschickt (Ping oder Daten)
            if (m_connection) {
                m_connection->partnerAlive(); // Watchdog zuruecksetzen
            }
            handleNetworkPulse(pulse.value.sival_int);
            break;

        case ipc::TIMER_TICK:
            handleTimerPulse(pulse.value.sival_int);
            break;

        case ipc::HEARTBEATTACKT:
            // Der 50ms-Timer sagt: Deadline pruefen!
            if (m_connection) {
                m_connection->heartbeatTackt();

                // Falls verbunden: Heartbeat an den Partner senden
                if (m_connection->isConnected()) {
                    MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::NETWORK_MSG, 0);
                }
            }
            break;

        case ipc::CONNECTION_MSG:
            // ConnectionManager hat den Partner gefunden
            handleConnectionPulse(pulse.value.sival_int);
            break;

        case ipc::DISCONNECT:
            // KRITISCH: Verbindung verloren oder Not-Halt
            std::cerr << "[Dispatcher] ACHTUNG: Verbindung unterbrochen! Not-Halt." << std::endl;
            if (m_actuators) m_actuators->driveStop();
            // Hier spaeter: m_logik->geheInFehlerzustand();
            break;

        case ipc::ANLAGE_2_CIID:
           m_partnerCoid =pulse.value.sival_int;
           break;

        default:
            std::cout << "[Dispatcher] Unbekannter Puls-Code: " << (int)pulse.code << std::endl;
            break;
    }
}


void Dispatcher::handleConnectionPulse(uint32_t payload) {
    m_partnerCoid = payload; // Die COID vom ConnectionManager speichern
    std::cout << "[Dispatcher] Partner-Verbindung aktiv (COID: " << m_partnerCoid << ")" << std::endl;

    // Asymmetrischer Start: Anlage 2 sagt Anlage 1 "Hallo"
    if (m_position == ipc::HINTEN_ANLAGE_2) {
        std::cout << "[Dispatcher] Sende Initial-Handshake an Anlage 1..." << std::endl;
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::ANLAGE_2_CIID, m_chid);
    }
}


// ============================================================================
// EVENT HANDLER LOKAL (HAL)
// ============================================================================
void Dispatcher::handleHalPulse(uint32_t payload) {
    uint16_t sensor_id = ipc::decodeSensorID(payload);
    uint16_t sensor_state = ipc::decodeSensorState(payload);

    std::cout << "[Dispatcher] Event -> Sensor: " << sensor_id << " | Zustand: " << sensor_state << std::endl;

    if (!m_actuators) return;

    // TASTE 1: START
    if (sensor_id == ipc::BTN_START) {
        if (sensor_state == 1) {
            std::cout << ">>> TEST: Start gedrueckt! Gruen blinkt! <<<" << std::endl;
            m_actuators->lightGreenBlinkFast();

            // Nachricht an Partner senden, falls verbunden
            if (m_partnerCoid != -1) {
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::NETWORK_MSG, payload);
                std::cout << "[" << getAnlagenPositionName() << "] Info an Partner gesendet!" << std::endl;
            } else {
                std::cout << "[" << getAnlagenPositionName() << "] Konnte Info nicht senden. Partner ist noch offline." << std::endl;
            }
        }
        else {
            std::cout << ">>> TEST: Start losgelassen! Gruen aus! <<<" << std::endl;
            m_actuators->lightGreenOff();

            // Nachricht an Partner senden, falls verbunden
            if (m_partnerCoid != -1) {
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::NETWORK_MSG, payload);
                std::cout << "[" << getAnlagenPositionName() << "] Info an Partner gesendet!" << std::endl;
            } else {
                std::cout << "[" << getAnlagenPositionName() << "] Konnte Info nicht senden. Partner ist noch offline." << std::endl;
            }
        }
    }
    // TASTE 2: RESET
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
    // TASTE 3: STOP
    else if (sensor_id == ipc::BTN_STOP) {
        if (sensor_state == 0) {
            std::cout << ">>> TEST: Stop gedrueckt! Rot blinkt! <<<" << std::endl;
            m_actuators->lightRedBlinkSlow();
        }
        else {
            std::cout << ">>> TEST: Stop losgelassen! Rot aus! <<<" << std::endl;
            m_actuators->lightRedOff();
        }
    }
    // LICHTSCHRANKE 1: Einlauf (Entry)
    else if (sensor_id == ipc::LS_ENTRY) {
        if (sensor_state == 0) {
            std::cout << ">>> TEST: Werkstueck am Einlauf erkannt! Motor faehrt RECHTS! <<<" << std::endl;
            m_actuators->driveRight();
            m_actuators->lightGreenBlinkSlow();
        }
    }
    // LICHTSCHRANKEN 2 & 3: Auslauf (End) oder Rutsche (Chute)
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

// ============================================================================
// EVENT HANDLER NETZWERK
// ============================================================================
void Dispatcher::handleNetworkPulse(uint32_t payload) {
    uint16_t sensor_id = ipc::decodeSensorID(payload);
    uint16_t sensor_state = ipc::decodeSensorState(payload);

    if (sensor_id == ipc::BTN_START) {
        if (sensor_state == 1) {
            std::cout << ">>> TEST (VON PARTNER): Start gedrueckt! Gruen blinkt! <<<" << std::endl;
            if (m_actuators) m_actuators->lightGreenBlinkFast();
        }
        else {
            std::cout << ">>> TEST (VON PARTNER): Start losgelassen! Gruen aus! <<<" << std::endl;
            if (m_actuators) m_actuators->lightGreenOff();
        }
    }
}

// ============================================================================
// EVENT HANDLER TIMER
// ============================================================================
void Dispatcher::handleTimerPulse(int timer_id) {
    std::cout << "[Dispatcher] TIMER abgelaufen! ID: " << timer_id << std::endl;
}
