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
Dispatcher::Dispatcher(bool pos) : m_chid(-1), partner_coid(-1), m_actuators(nullptr) {
    setAnlagenPosition(pos);
    registingNameInGNS();

    if (m_chid == -1) {
        std::cerr << "KRITISCHER FEHLER: Dispatcher konnte QNX Channel nicht erstellen! "
                  << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "[Dispatcher] QNX Channel erfolgreich erstellt. CHID: " << m_chid << std::endl;
}

Dispatcher::~Dispatcher() {
    if (m_chid != -1) {
        ChannelDestroy(m_chid);
        std::cout << "[Dispatcher] QNX Channel zerstoert." << std::endl;
    }
    if (partner_coid != -1) {
        name_close(partner_coid);
    }
}

// ============================================================================
// INITIALISIERUNG & NETZWERK
// ============================================================================
void Dispatcher::setAnlagenPosition(bool pos) {
    if (pos) {
        m_position = AnlagenPosition::VORN_ANLAGE_1;
        myName = "Anlage_Vorn";
        partnerName = "Anlage_Hinten";
    } else {
        m_position = AnlagenPosition::HINTEN_ANLAGE_2;
        myName = "Anlage_Hinten";
        partnerName = "Anlage_Vorn";
    }
}

void Dispatcher::registingNameInGNS() {
    if (partner_coid != -1) {
        return;
    }

    name_attach_t* attach = name_attach(NULL, myName.c_str(), NAME_FLAG_ATTACH_GLOBAL);

    if (attach == NULL) {
        std::cerr << "[" << myName << "] Fehler: Name konnte nicht registriert werden!" << std::endl;
    } else {
        std::cout << "[" << myName << "] Ich bin nun im Netzwerk erreichbar." << std::endl;
        m_chid = attach->chid;
    }
}

void Dispatcher::searchForNeighbor() {
    if (partner_coid != -1) {
        return;
    }

    std::cout << "[" << myName << "] Suche Partner: " << partnerName << "..." << std::endl;

    std::string globalPath = "/dev/name/global/" + partnerName;
    partner_coid = name_open(globalPath.c_str(), 0);

    if (partner_coid == -1) {
        std::cout << "[" << myName << "] Partner noch nicht online. Versuche es später erneut." << std::endl;
    } else {
        std::cout << "===========================================" << std::endl;
        std::cout << ">>> [" << myName << "] NETZWERK-VERBINDUNG ZU " << partnerName << " HERGESTELLT! COID: " << partner_coid << " <<<" << std::endl;
        std::cout << "===========================================\n" << std::endl;
    }
}

// ============================================================================
// DIE ENDLOSSCHLEIFE (REAKTOR)
// ============================================================================
void Dispatcher::run() {
    std::cout << "[Dispatcher] Starte Event-Loop. Warte auf Pulse und Nachrichten..." << std::endl;

    union {
        struct _pulse pulse;
        char buffer[256];
    } msg;

    while (true) {
        int rcvid = MsgReceive(m_chid, &msg, sizeof(msg), nullptr);

        if (rcvid == -1) {
            std::cerr << "[Dispatcher] Fehler bei MsgReceive!" << std::endl;
            continue;
        }

        // FALL 1: ES IST EIN PULSE (rcvid ist 0)
        if (rcvid == 0) {
            switch (msg.pulse.code) {
                case ipc::HAL_INPUT:
                    handleHalPulse(msg.pulse.value.sival_int);
                    break;

                case ipc::NETWORK_MSG:
                    std::cout << "\n>>> NETZWERK POST ERHALTEN <<<" << std::endl;
                    handleNetworkPulse(msg.pulse.value.sival_int);
                    break;

                case _PULSE_CODE_DISCONNECT:
                    std::cout << "[Dispatcher] Partner hat die Verbindung getrennt." << std::endl;
                    partner_coid = -1;
                    break;

                default:
                    break;
            }
        }
        // FALL 2: ES IST EINE NACHRICHT (rcvid ist größer als 0)
        else if (rcvid > 0) {
            MsgReply(rcvid, EOK, nullptr, 0);
        }
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

            searchForNeighbor();

            if (partner_coid != -1) {
                MsgSendPulse(partner_coid, -1, ipc::NETWORK_MSG, payload);
                std::cout << "[" << myName << "] Info an " << partnerName << " gesendet!" << std::endl;
            } else {
                std::cout << "[" << myName << "] Konnte Info nicht senden. " << partnerName << " ist noch offline." << std::endl;
            }
        }
        else {
            std::cout << ">>> TEST: Start losgelassen! Gruen aus! <<<" << std::endl;
            m_actuators->lightGreenOff();

            searchForNeighbor(); // Auch beim Loslassen sicherheitshalber prüfen

            if (partner_coid != -1) {
                MsgSendPulse(partner_coid, -1, ipc::NETWORK_MSG, payload);
                std::cout << "[" << myName << "] Info an " << partnerName << " gesendet!" << std::endl;
            } else {
                std::cout << "[" << myName << "] Konnte Info nicht senden. " << partnerName << " ist noch offline." << std::endl;
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