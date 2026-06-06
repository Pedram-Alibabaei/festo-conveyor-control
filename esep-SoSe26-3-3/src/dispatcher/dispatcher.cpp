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
#include <sys/dispatch.h>

// ============================================================================
// KONSTRUKTOR & DESTRUKTOR
// ============================================================================
Dispatcher::Dispatcher()
    : m_chid(-1),
      m_localCoid(-1),
      m_partnerCoid(-1),
      m_attachHandle(nullptr),
      m_position(AnlagenPosition::UNKNOWN_POSITION),
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
    std::string myName = (pos == AnlagenPosition::VORN_ANLAGE_1) ? "Anlage_1" : "Anlage_2";

    m_attachHandle = name_attach(NULL, myName.c_str(), NAME_FLAG_ATTACH_GLOBAL);
    if (m_attachHandle == nullptr) {
        std::cerr << "[Dispatcher] Fehler: name_attach fehlgeschlagen!" << std::endl;
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
                    // m_logik->processEvent(logik::Event::ESTOP_PRESSED); // <--- AUSKOMMENTIEREN!
                    std::cout << "[Dispatcher] WARNUNG: Netzwerk weg. Ignoriert fuer Solo-Test!" << std::endl;
                    break;

        // ====================================================================
        // TIMER EVENTS
        // ====================================================================
        case ipc::BELT_TICK:
            m_logik->tackt();
            break;

        case ipc::HEARTBEATTACKT:
            if (m_connection) {
                m_connection->heartbeatTackt();
                if (m_connection->isConnected()) {
                    MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::HEARTBEAT, 0);
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
            std::cerr << "[Dispatcher] Warnung: Unbekannter Puls-Code: " << (int)pulse.code << std::endl;
            break;
}
}

void Dispatcher::handleConnectionPulse(uint32_t payload) {
    m_partnerCoid = payload;

    if (m_position == HINTEN_ANLAGE_2) {
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::ANLAGE_2_CIID, 1);
    }
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
    }

    // 2. LICHTSCHRANKEN & SENSOREN (BLOCKED / CLEARED)
    else if (sensor_id == ipc::LS_ENTRY) {
        m_logik->processEvent(sensor_state == ipc::State::BLOCKED ? logik::Event::ENTRY_BLOCKED : logik::Event::ENTRY_CLEARED);
    }
    else if (sensor_id == ipc::HEIGHT) {
        if (sensor_state == ipc::State::BLOCKED) {
            m_logik->processEvent(logik::Event::HEIGHT_BLOCKED);
        }
        //else if (sensor_state == ipc::State::CLEARED) {
        //    m_logik->processEvent(logik::Event::HEIGHT_CLEARED);
        } else if (sensor_state == ipc::State::HIGH_ERROR) {
            m_logik->processEvent(logik::Event::HEIGHT_ERROR);
        }else {
        	m_logik->workpieceTypeFromHS(static_cast<WpType>(sensor_state));
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

    if (sensor_id == ipc::BTN_START) {
        m_logik->processEvent(message_type == ipc::State::PRESSED ? logik::Event::PARTNER_START_PRESSED : logik::Event::PARTNER_START_RELEASED);
    }
    else if (sensor_id == ipc::NetworkMessage::REQ_RECEIVE) {
        m_logik->processEvent(logik::Event::REQ_TO_GIVE_WP);
    }
    else if (sensor_id == ipc::NetworkMessage::READY_RECEIVE) {
        m_logik->processEvent(logik::Event::READY_TO_RECEIVE_WP);
    }
    else if (sensor_id == ipc::NetworkMessage::CHUTE){

        bool isFull = (message_type == ipc::VOLL);
        if (m_logik != nullptr) {
            m_logik->setPartnerChuteStatus(isFull);
        }

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

    std::cout << "[Dispatcher] Komplette Kalibrierungs-Map an Manager uebergeben und gespeichert." << std::endl;
}


/**
 * @brief Prueft über den CalibrationManager, ob die XML/Text-Datei mit den
 * gelernten Ticks und ADC-Fenstern auf der Festplatte existiert.
 */
bool Dispatcher::requestFileExists() {
    return m_calibrationManager->fileExists();
    return false;
}

void Dispatcher::sendeChuteStats(bool isVoll) {
    if (m_partnerCoid == -1) {return;}
    ipc::State state = isVoll ? ipc::VOLL : ipc::EMPTY;
    uint32_t payload = ipc::encodeHalPayload(ipc::NetworkMessage::CHUTE, state);
    MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_MSG, payload);
}
