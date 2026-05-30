/**
 * @file dispatcher.cpp
 * @brief Implementierung des zentralen Nachrichtenverteilers als reiner Postbote.
 */

#include "dispatcher.hpp"
#include "shared/ipc_definitions.hpp"
#include "shared/logikEvent.hpp"
#include "logik/i_Logik.hpp"
#include "logik/logik.hpp"
#include <iostream>
#include <cerrno>
#include <cstring>
#include <sys/neutrino.h>
#include <thread>
#include <sys/dispatch.h>

// ============================================================================
// KONSTRUKTOR & DESTRUKTOR
// ============================================================================
Dispatcher::Dispatcher()
    : m_chid(-1),
      m_localCoid(-1),
      m_partnerCoid(-1),
      m_attachHandle(nullptr),
      m_position(UNKNOWN_POSITION),
      m_running(false),
      m_actuators(nullptr),
      m_sensors(nullptr),
      m_connection(nullptr),          // Netzwerk-Manager
      m_calibrationManager(nullptr),   // KORRIGIERT: Kalibrierungs-Manager Pointer
      m_timer(nullptr),
      m_logik(nullptr)
{
}

Dispatcher::~Dispatcher() {
    stop();

    if (m_localCoid != -1) ConnectDetach(m_localCoid);
    if (m_partnerCoid != -1) ConnectDetach(m_partnerCoid);
    if (m_chid != -1) ChannelDestroy(m_chid);
}

// ============================================================================
// Initialisierung
// ============================================================================
void Dispatcher::init(AnlagenPosition pos) {
    m_position = pos;
    std::string myName = (pos == VORN_ANLAGE_1) ? "Anlage_1" : "Anlage_2";

    m_attachHandle = name_attach(NULL, myName.c_str(), NAME_FLAG_ATTACH_GLOBAL);
    if (m_attachHandle == nullptr) {
       // std::cerr << "[Dispatcher] Fehler: name_attach fehlgeschlagen!" << std::endl;
        return;
    }

    m_chid = m_attachHandle->chid;
    m_localCoid = ConnectAttach(0, 0, m_chid, _NTO_SIDE_CHANNEL, 0);
}

void Dispatcher::stop() {
    m_running = false;
}

// ============================================================================
// Hauptschleife (Der Reaktor)
// ============================================================================
void Dispatcher::run() {
    union {
        struct _pulse pulse;
        uint16_t type;
        uint8_t msg_buffer[256];
    } msg;

    m_running = true;

    while (m_running) {
        int rcvid = MsgReceive(m_chid, &msg, sizeof(msg), NULL);

        if (rcvid == -1) {
            if (errno == EINTR) continue;
            break;
        }

        if (rcvid == 0) {
            handlePulse(msg.pulse);
        } else {
            if (msg.type == _IO_CONNECT) {
                MsgReply(rcvid, EOK, NULL, 0);
            } else {
                MsgError(rcvid, ENOSYS);
            }
        }
    }
}

// ============================================================================
// Zentraler Event-Verteiler
// ============================================================================
void Dispatcher::handlePulse(const struct _pulse& pulse) {
    if (!m_logik) return;

    switch (pulse.code) {

        // ====================================================================
        // LOKALE HARDWARE & SYSTEM
        // ====================================================================
        case ipc::HAL_INPUT:
            handleHalPulse(pulse.value.sival_int);
            break;

        case ipc::HAL_ESTOP:
            handleHalEStopPulse(pulse.value.sival_int);
            break;

        case ipc::PulseCode::HAL_ADC_CALIBRATION:
            {
                uint16_t calib_value = (pulse.value.sival_int >> 16) & 0xFFFF;
                uint16_t calib_mode  = pulse.value.sival_int & 0xFFFF;
                m_logik->receiveRawCalibrationData(calib_mode, calib_value);
            }
            break;

        case ipc::PulseCode::MANAGER_ADC_CALIBRATION:
            // Daten kommen beim Booten aus dem CalibrationManager -> Direkt an die Sensorik (ProfileAnalyzer)
            if (m_sensors) {
                uint16_t calib_value = (pulse.value.sival_int >> 16) & 0xFFFF;
                uint16_t calib_mode  = pulse.value.sival_int & 0xFFFF;
                m_sensors->updateCalibrationThresholds(calib_mode, calib_value);
            }
            break;

        case ipc::PulseCode::MANAGER_TICK_CALIBRATION:
            // Ticks kommen beim Booten aus dem CalibrationManager -> Direkt an die FSM-Zeiten der Logik
            {
                uint16_t tick_value = (pulse.value.sival_int >> 16) & 0xFFFF;
                uint16_t calib_mode = pulse.value.sival_int & 0xFFFF;

                // Nutzt den neuen CalibMode-Subtyp (z.B. TICK_END_B_TO_END_C)
                m_logik->setTrackedSegmentTicks(calib_mode, tick_value);
            }
            break;

        case ipc::CONNECTION_MSG:
            handleConnectionPulse(pulse.value.sival_int);
            break;

        case ipc::DISCONNECT:
           // std::cout << "[Dispatcher] Netzwerkverbindung verloren!" << std::endl;
            m_partnerCoid = -1; // WICHTIG: Connection-Handle löschen, damit wir nicht ins Leere senden
            m_logik->processEvent(logik::Event::CONNECTION_LOST);
            break;

        // ====================================================================
        // TIMER EVENTS
        // ====================================================================
        case ipc::BELT_TICK:
            m_logik->tackt();
            break;

        case ipc::HEARTBEATTACKT:
            if (m_connection) {
                // 1. Lokalen Zähler aktualisieren (prüft, ob der Partner sich gemeldet hat)
                m_connection->heartbeatTackt();

                // 2. Ping an den Partner senden, OHNE den Dispatcher zu blockieren
                if (m_connection->isConnected() && m_partnerCoid != -1) {

                    // Wir kopieren die COID, damit der Thread sie sicher nutzen kann
                    int targetCoid = m_partnerCoid;

                    // Wir starten einen "Fire-and-Forget"-Thread.
                    // Wenn QNX diesen Thread jetzt 5 Sekunden lang einfriert,
                    // stört das unseren Dispatcher überhaupt nicht mehr!
                    std::thread([targetCoid]() {
                        MsgSendPulse(targetCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::HEARTBEAT, 0);
                    }).detach();
                }
            }
            break;

        case ipc::SORTER_TIMER_TICK:
            m_logik->processEvent(logik::Event::EJECTOR_CLEARED);
            break;

        // ====================================================================
        // NETZWERK BASIS
        // ====================================================================
        case ipc::HEARTBEAT:
            if (m_connection && m_connection->isConnected()){
                m_connection->partnerAlive();
            }
            break;

        case ipc::PARTNER_MSG:
            if (m_connection) m_connection->partnerAlive();
            handleNetworkPulse(pulse.value.sival_int);
            break;

        case ipc::PARTNER_ESTOP:
            handlePartnerEStopPulse(pulse.value.sival_int);
            break;

        case ipc::ANLAGE_2_CIID:
            if (m_connection && !m_connection->isConnected()) {
                m_connection->connectToPartner();
            }
            break;

        // ====================================================================
        // NETZWERK WERKSTÜCK-ÜBERGABE
        // ====================================================================
        case ipc::WP_INFO: {
            int id = (pulse.value.sival_int >> 16) & 0xFFFF;
            WpType type = static_cast<WpType>(pulse.value.sival_int & 0xFFFF);
            m_logik->workpieceInfo(id, type);
            break;
        }

        case ipc::UPDATE_LIST: {
            WpType type = static_cast<WpType>(pulse.value.sival_int);
            m_logik->updateWpListe(type);
            break;
        }

        default:
           // std::cerr << "[Dispatcher] Warnung: Unbekannter Puls-Code: " << (int)pulse.code << std::endl;
            break;
}
}

void Dispatcher::handleConnectionPulse(uint32_t payload) {
    m_partnerCoid = payload;

    if (m_position == HINTEN_ANLAGE_2) {
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::ANLAGE_2_CIID, 1);
    }

    // NEU: Dem System mitteilen, dass die Verbindung (wieder) etabliert ist
    m_logik->processEvent(logik::Event::CONNECTION_RESTORED);
}

// ============================================================================
// EVENT HANDLER LOKAL (HAL)
// ============================================================================
void Dispatcher::handleHalPulse(uint32_t payload) {
    uint16_t sensor_id = ipc::decodeSensorID(payload);
    uint16_t sensor_state = ipc::decodeSensorState(payload);

    // 1. TASTER (PRESSED / RELEASED)
    if (sensor_id == ipc::BTN_START) {
        m_logik->processEvent(sensor_state == ipc::State::PRESSED ? logik::Event::START_PRESSED : logik::Event::START_RELEASED);

        if (m_partnerCoid != -1) {
            MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_MSG, payload);
        }
    }
    else if (sensor_id == ipc::BTN_STOP) {
        m_logik->processEvent(sensor_state == ipc::State::PRESSED ? logik::Event::STOP_PRESSED : logik::Event::STOP_RELEASED);
    }
    else if (sensor_id == ipc::BTN_RESET) {
        m_logik->processEvent(sensor_state == ipc::State::PRESSED ? logik::Event::RESET_PRESSED : logik::Event::RESET_RELEASED);
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_MSG, payload);
    }

    // 2. LICHTSCHRANKEN & SENSOREN (BLOCKED / CLEARED)
    else if (sensor_id == ipc::LS_ENTRY) {
        m_logik->processEvent(sensor_state == ipc::State::BLOCKED ? logik::Event::ENTRY_BLOCKED : logik::Event::ENTRY_CLEARED);
    }
    else if (sensor_id == ipc::HEIGHT) {
        if (sensor_state == ipc::State::BLOCKED) {
            m_logik->processEvent(logik::Event::HEIGHT_BLOCKED);
        } else if (sensor_state == ipc::State::CLEARED) {
            m_logik->processEvent(logik::Event::HEIGHT_CLEARED);
        } else if (sensor_state == ipc::State::HIGH_ERROR) {
            m_logik->processEvent(logik::Event::HEIGHT_ERROR);
        }
    }
    else if (sensor_id == ipc::SENSOR_METAL) {
        m_logik->processEvent(sensor_state == ipc::State::BLOCKED ? logik::Event::METAL_DETECTED : logik::Event::METAL_CLEARED);
    }
    else if (sensor_id == ipc::LS_SWITCH) {
        m_logik->processEvent(sensor_state == ipc::State::BLOCKED ? logik::Event::EJECTOR_BLOCKED : logik::Event::EJECTOR_CLEARED);
    }
    else if (sensor_id == ipc::LS_END) {
        m_logik->processEvent(sensor_state == ipc::State::BLOCKED ? logik::Event::END_BLOCKED : logik::Event::END_CLEARED);
    }
    else if (sensor_id == ipc::LS_CHUTE) {
        m_logik->processEvent(sensor_state == ipc::State::BLOCKED ? logik::Event::CHUTE_BLOCKED : logik::Event::CHUTE_CLEARED);
    }
}

// ============================================================================
// EVENT HANDLER NETZWERK
// ============================================================================
void Dispatcher::handleNetworkPulse(uint32_t payload) {
    uint16_t sensor_id = ipc::decodeSensorID(payload);
    uint16_t message_type = ipc::decodeSensorState(payload);

    // Wenn der Partner START drückt oder loslässt
    if (sensor_id == ipc::BTN_START) {
        m_logik->processEvent(message_type == ipc::State::PRESSED ? logik::Event::PARTNER_START_PRESSED : logik::Event::PARTNER_START_RELEASED);
    }
    // NEU: Wenn der Partner im EStopUnlockedState den RESET drückt
    else if (sensor_id == ipc::BTN_RESET && message_type == ipc::State::PRESSED) {
      //  std::cout << "[Dispatcher] Netzwerk-Event: Partner fordert synchronisierten Reset!" << std::endl;
        m_logik->processEvent(logik::Event::PARTNER_RESET_PRESSED);
    }
    // Werkstückübergabe-Protokoll
    else if (message_type == ipc::NetworkMessage::REQ_RECEIVE) {
        m_logik->processEvent(logik::Event::REQ_TO_GIVE_WP);
    }
    else if (message_type == ipc::NetworkMessage::READY_RECEIVE) {
        m_logik->processEvent(logik::Event::READY_TO_RECEIVE_WP);
    }
}

// ============================================================================
// DURCHREICHE-METHODEN FÜR DAS NETZWERK
// ============================================================================
void Dispatcher::workpieceInfo(int id, WpType type) {
    if (m_partnerCoid != -1) {
        uint32_t payload = (static_cast<uint32_t>(id) << 16) | static_cast<uint32_t>(type);
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::WP_INFO, payload);
    }
}

void Dispatcher::RequestToReceiveWp() {
    if (m_partnerCoid != -1) {
        uint32_t payload = ipc::encodeHalPayload(0, ipc::NetworkMessage::REQ_RECEIVE);
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_MSG, payload);
    }
}

void Dispatcher::readyToReceiveWp() {
    if (m_partnerCoid != -1) {
        uint32_t payload = ipc::encodeHalPayload(0, ipc::NetworkMessage::READY_RECEIVE);
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_MSG, payload);
    }
}

void Dispatcher::updateWpListe(WpType type) {
    if (m_partnerCoid != -1) {
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::UPDATE_LIST, static_cast<uint32_t>(type));
    }
}

// ============================================================================
// SYNCHRONISIERTER RESET (Master-Master Brücke)
// ============================================================================
void Dispatcher::sendPartnerResetPressed(){
    if (m_partnerCoid != -1) {
        // Wir verpacken die Sensor-ID (BTN_RESET) und einen eindeutigen Netzwerk-Zustand.
        // Dafür nutzen wir das bestehende PARTNER_MSG System.
        // Hinweis: ipc::State::PRESSED/RELEASED wird hier zweckentfremdet, um die Anfrage zu signalisieren.
        uint32_t payload = ipc::encodeHalPayload(ipc::BTN_RESET, ipc::State::PRESSED);

        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_MSG, payload);
      //  std::cout << "[Dispatcher] Reset-Anfrage erfolgreich an Partner gesendet." << std::endl;
    }
}


// ============================================================================
// LOKALER E-STOP
// ============================================================================
void Dispatcher::handleHalEStopPulse(uint32_t payload) {
    uint16_t sensor_id = ipc::decodeSensorID(payload);
    uint16_t sensor_state = ipc::decodeSensorState(payload);

    if (sensor_id == ipc::BTN_ESTOP) {
        if (sensor_state == ipc::State::RELEASED) {
            m_logik->processEvent(logik::Event::ESTOP_PRESSED);

            if (m_partnerCoid != -1) {
                uint32_t netPayload = ipc::encodeHalPayload(ipc::BTN_ESTOP, ipc::State::RELEASED);
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_ESTOP, netPayload);
            }
        }
        else if (sensor_state == ipc::State::PRESSED) {
            m_logik->processEvent(logik::Event::ESTOP_RELEASED);

            if (m_partnerCoid != -1) {
                uint32_t netPayload = ipc::encodeHalPayload(ipc::BTN_ESTOP, ipc::State::PRESSED);
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_ESTOP, netPayload);
            }
        }
    }
}

// ============================================================================
// PARTNER E-STOP
// ============================================================================
void Dispatcher::handlePartnerEStopPulse(uint32_t payload) {
    uint16_t partner_state = ipc::decodeSensorState(payload);

    if (partner_state == ipc::State::RELEASED) {
        m_logik->processEvent(logik::Event::PARTNER_ESTOP_PRESSED);
    }
    else if (partner_state == ipc::State::PRESSED) {
        m_logik->processEvent(logik::Event::PARTNER_ESTOP_RELEASED);
    }
}

// ============================================================================
// KALIBRIERUNGS-SCHNITTSTELLE (KORRIGIERT)
// ============================================================================
void Dispatcher::saveCompleteCalibrationMap(const std::map<std::string, int>& calibData) {
    if (!m_calibrationManager) return; // Greift nun sauber auf das m_calibrationManager-Member zu

    // 1. Alle Werte aus der Logik-Map in den CalibrationManager übertragen
    for (const auto& pair : calibData) {
        m_calibrationManager->setInt(pair.first, pair.second);
    }

    // 2. Die Map permanent in die Textdatei schreiben
    m_calibrationManager->save();

   // std::cout << "[Dispatcher] Komplette Kalibrierungs-Map an Manager uebergeben und gespeichert." << std::endl;
}
