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

    if (!m_logik) return;

    // TASTE 1: START
    if (sensor_id == ipc::BTN_START) {
        if (sensor_state == 1) {
            m_logik->processEvent(logik::Event::START_PRESSED);
            // Nachricht an Partner senden, falls verbunden
            if (m_partnerCoid != -1) {
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::NETWORK_MSG, payload);
            }
        }
        else {
            m_logik->processEvent(logik::Event::START_RELEASED);
            if (m_partnerCoid != -1) {
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::NETWORK_MSG, payload);
            }
        }
    }
    // TASTE 2: RESET
    else if (sensor_id == ipc::BTN_RESET) {
        if (sensor_state == 1) {
            m_logik->processEvent(logik::Event::RESET_PRESSED);
        }
        else {
            m_logik->processEvent(logik::Event::RESET_RELEASED);
        }
    }
    // TASTE 3: STOP
    else if (sensor_id == ipc::BTN_STOP) {
        if (sensor_state == 0) { // Active Low
            m_logik->processEvent(logik::Event::STOP_PRESSED);
        }
        else {
            m_logik->processEvent(logik::Event::STOP_RELEASED);
        }
    }
    // TASTE 4: E-STOP (REQ_33, REQ_37)
    else if (sensor_id == ipc::BTN_ESTOP) {
        if (sensor_state == 0) { // Active Low
            m_logik->processEvent(logik::Event::ESTOP_PRESSED);
            if (m_partnerCoid != -1) {
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::NETWORK_MSG, payload);
            }
        }
        else {
            m_logik->processEvent(logik::Event::ESTOP_RELEASED);
            if (m_partnerCoid != -1) {
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::NETWORK_MSG, payload);
            }
        }
    }
    // LICHTSCHRANKE 1: Einlauf (Entry)
    else if (sensor_id == ipc::LS_ENTRY) {
        if (sensor_state == 0) {
            m_logik->processEvent(logik::Event::ENTRY_BLOCKED);
        } else {
            m_logik->processEvent(logik::Event::ENTRY_CLEARED);
        }
    }
    // LICHTSCHRANKEN 2 & 3: Auslauf (End) oder Rutsche (Chute)
    else if (sensor_id == ipc::LS_END) {
        if (sensor_state == 0) {
            m_logik->processEvent(logik::Event::END_BLOCKED);
        } else {
            m_logik->processEvent(logik::Event::END_CLEARED);
        }
    }
    else if (sensor_id == ipc::LS_CHUTE) {
        if (sensor_state == 0) {
            m_logik->processEvent(logik::Event::CHUTE_BLOCKED);
        } else {
            m_logik->processEvent(logik::Event::CHUTE_CLEARED);
        }
    }
}

// ============================================================================
// EVENT HANDLER NETZWERK
// ============================================================================
void Dispatcher::handleNetworkPulse(uint32_t payload) {
    uint16_t sensor_id = ipc::decodeSensorID(payload);
    uint16_t sensor_state = ipc::decodeSensorState(payload);

    if (!m_logik) return;

    if (sensor_id == ipc::BTN_START) {
        if (sensor_state == 1) {
            m_logik->processEvent(logik::Event::NET_START_PRESSED);
        }
        else {
            m_logik->processEvent(logik::Event::NET_START_RELEASED);
        }
    }
    else if (sensor_id == ipc::BTN_ESTOP) {
        if (sensor_state == 0) { // Active Low
            m_logik->processEvent(logik::Event::NET_ESTOP_PRESSED);
        }
        else {
            m_logik->processEvent(logik::Event::NET_ESTOP_RELEASED);
        }
    }
}

// ============================================================================
// EVENT HANDLER TIMER
// ============================================================================
void Dispatcher::handleTimerPulse(int timer_id) {
    std::cout << "[Dispatcher] TIMER abgelaufen! ID: " << timer_id << std::endl;
}

// ============================================================================
// I_DISPATCHER IMPLEMENTIERUNG (Weiterleitung an Sub-Komponenten)
// ============================================================================
void Dispatcher::startBeltClock(int ms) {
    // Wenn es einen i_Timer gibt, dann dort starten
}

void Dispatcher::workpieceInfo(int id, WpType type) {}
void Dispatcher::RequestToReceiveWp() {}
void Dispatcher::readyToReceiveWp() {}
void Dispatcher::updateWpListe(WpType type) {}

void Dispatcher::driveRight() { if(m_actuators) m_actuators->driveRight(); }
void Dispatcher::driveLeft() { if(m_actuators) m_actuators->driveLeft(); }
void Dispatcher::driveSlowRight() { if(m_actuators) m_actuators->driveSlowRight(); }
void Dispatcher::driveSlowLeft() { if(m_actuators) m_actuators->driveSlowLeft(); }
void Dispatcher::driveStop() { if(m_actuators) m_actuators->driveStop(); }

void Dispatcher::openJunction() { if(m_actuators) m_actuators->openJunction(); }
void Dispatcher::closeJunction() { if(m_actuators) m_actuators->closeJunction(); }

void Dispatcher::lightGreenOn() { if(m_actuators) m_actuators->lightGreenOn(); }
void Dispatcher::lightGreenOff() { if(m_actuators) m_actuators->lightGreenOff(); }
void Dispatcher::lightGreenBlinkFast() { if(m_actuators) m_actuators->lightGreenBlinkFast(); }
void Dispatcher::lightGreenBlinkSlow() { if(m_actuators) m_actuators->lightGreenBlinkSlow(); }

void Dispatcher::lightYellowOn() { if(m_actuators) m_actuators->lightYellowOn(); }
void Dispatcher::lightYellowOff() { if(m_actuators) m_actuators->lightYellowOff(); }
void Dispatcher::lightYellowBlinkFast() { if(m_actuators) m_actuators->lightYellowBlinkFast(); }
void Dispatcher::lightYellowBlinkSlow() { if(m_actuators) m_actuators->lightYellowBlinkSlow(); }

void Dispatcher::lightRedOn() { if(m_actuators) m_actuators->lightRedOn(); }
void Dispatcher::lightRedOff() { if(m_actuators) m_actuators->lightRedOff(); }
void Dispatcher::lightRedBlinkFast() { if(m_actuators) m_actuators->lightRedBlinkFast(); }
void Dispatcher::lightRedBlinkSlow() { if(m_actuators) m_actuators->lightRedBlinkSlow(); }

void Dispatcher::turnLEDStartOn() { if(m_actuators) m_actuators->turnLEDStartOn(); }
void Dispatcher::turnLEDStartOff() { if(m_actuators) m_actuators->turnLEDStartOff(); }
void Dispatcher::turnLEDStartBlinkFast() { if(m_actuators) m_actuators->turnLEDStartBlinkFast(); }
void Dispatcher::turnLEDStartBlinkSlow() { if(m_actuators) m_actuators->turnLEDStartBlinkSlow(); }

void Dispatcher::turnLEDResetOn() { if(m_actuators) m_actuators->turnLEDResetOn(); }
void Dispatcher::turnLEDResetOff() { if(m_actuators) m_actuators->turnLEDResetOff(); }
void Dispatcher::turnLEDResetBlinkFast() { if(m_actuators) m_actuators->turnLEDResetBlinkFast(); }
void Dispatcher::turnLEDResetBlinkSlow() { if(m_actuators) m_actuators->turnLEDResetBlinkSlow(); }

void Dispatcher::turnLEDQ1On() { if(m_actuators) m_actuators->turnLEDQ1On(); }
void Dispatcher::turnLEDQ1Off() { if(m_actuators) m_actuators->turnLEDQ1Off(); }
void Dispatcher::turnLEDQ1BlinkFast() { if(m_actuators) m_actuators->turnLEDQ1BlinkFast(); }
void Dispatcher::turnLEDQ1BlinkSlow() { if(m_actuators) m_actuators->turnLEDQ1BlinkSlow(); }

void Dispatcher::turnLEDQ2On() { if(m_actuators) m_actuators->turnLEDQ2On(); }
void Dispatcher::turnLEDQ2Off() { if(m_actuators) m_actuators->turnLEDQ2Off(); }
void Dispatcher::turnLEDQ2BlinkFast() { if(m_actuators) m_actuators->turnLEDQ2BlinkFast(); }
void Dispatcher::turnLEDQ2BlinkSlow() { if(m_actuators) m_actuators->turnLEDQ2BlinkSlow(); }

void Dispatcher::turnAllOff() { if(m_actuators) m_actuators->turnAllOff(); }
