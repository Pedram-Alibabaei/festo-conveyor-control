/**
 * @file dispatcher.cpp
 * @brief Implementierung des zentralen Nachrichtenverteilers als reiner Postbote.
 */

#include "dispatcher.hpp"
#include "shared/ipc_definitions.hpp"
#include "shared/logikEvent.hpp"
#include "logik/i_Logik.hpp"
#include "logik/logik.hpp"
#include "calibrationManager/i_CalibrationManager.hpp"
#include <iostream>
#include <cerrno>
#include <cstring>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

CalibrationData Dispatcher::s_incomingServiceData;
uint16_t Dispatcher::s_liveEmptyBelt = 0;
uint16_t Dispatcher::s_liveFlatCenter = 0;
uint32_t Dispatcher::s_liveMinSamples = 0;

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
      m_logik(nullptr),
      m_partnerEstopPressed(false)
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
    if (m_localCoid == -1) {
        std::cerr << "[Dispatcher] Fehler: ConnectAttach fehlgeschlagen!" << std::endl;
        return;
    }
    std::cout << "[Dispatcher] Registriert als '" << myName << "' (chid=" << m_chid
              << ", coid=" << m_localCoid << ")." << std::endl;
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
    switch (pulse.code) {
        case ipc::CONNECTION_MSG:
            handleConnectionPulse(pulse.value.sival_int);
            return;

        case ipc::HAL_INPUT:
            if (!m_logik) {
                std::cerr << "[Dispatcher] HAL-Event ohne Logik (code=" << static_cast<int>(pulse.code)
                          << ") – Event verworfen." << std::endl;
                return;
            }
            handleHalPulse(pulse.value.sival_int);
            return;

        case ipc::HAL_ESTOP:
            if (!m_logik) {
                return;
            }
            handleHalEStopPulse(pulse.value.sival_int);
            return;

        default:
            break;
    }

    if (!m_logik) {
        return;
    }

    switch (pulse.code) {

        // ====================================================================
        // LOKALE HARDWARE & SYSTEM
        // ====================================================================
        case ipc::HAL_ADC_CALIBRATION:
            {
            handleCalibrationPulse(pulse.value.sival_int);
            }
            break;

        case ipc::DISCONNECT:
            std::cout << "[Dispatcher] Netzwerkverbindung verloren." << std::endl;
            m_partnerCoid = -1;
            if (m_logik != nullptr) {
                m_logik->processEvent(logik::Event::CONNECTION_LOST);
            }
            break;

        // ====================================================================
        // TIMER EVENTS
        // ====================================================================
        case ipc::BELT_TICK:
            m_logik->takt();
            break;

        case ipc::HEARTBEATTACKT:
            if (m_connection) {
                m_connection->heartbeatTackt();
                if (m_connection->isConnected()) {
                    MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::HEARTBEAT, 0);
                }
            }
            break;

        // ====================================================================
        // NETZWERK BASIS
        // ====================================================================
        case ipc::HEARTBEAT:
            if (m_connection) {
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

    if (m_position == AnlagenPosition::HINTEN_ANLAGE_2) {
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::ANLAGE_2_CIID, 1);
    }

    std::cout << "[Dispatcher] Partner verbunden (coid=" << m_partnerCoid << ")." << std::endl;

    requestPartnerEstopStatus();
    sendLocalEstopStatusToPartner();

    if (m_logik != nullptr) {
        std::cout << "[Dispatcher] Partner-ESTOP (Cache vor RESTORED): "
                  << (isPartnerEstopPressed() ? "GEDRUECKT" : "NICHT gedrueckt") << std::endl;
        m_logik->processEvent(logik::Event::CONNECTION_RESTORED);
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
        std::cout << "[Dispatcher] START-Taster: "
                  << (sensor_state == ipc::State::PRESSED ? "PRESSED" : "RELEASED") << std::endl;
        m_logik->processEvent(sensor_state == ipc::State::PRESSED ? logik::Event::START_PRESSED : logik::Event::START_RELEASED);

        if (m_partnerCoid != -1) {
            MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_MSG, payload);
        }
    }
    else if (sensor_id == ipc::BTN_STOP) {
        std::cout << "[Dispatcher] STOP-Taster: "
                  << (sensor_state == ipc::State::PRESSED ? "PRESSED" : "RELEASED") << std::endl;
        m_logik->processEvent(sensor_state == ipc::State::PRESSED ? logik::Event::STOP_PRESSED : logik::Event::STOP_RELEASED);
    }
    else if (sensor_id == ipc::BTN_RESET) {
        std::cout << "[Dispatcher] RESET-Taster: "
                  << (sensor_state == ipc::State::PRESSED ? "PRESSED" : "RELEASED") << std::endl;
        m_logik->processEvent(sensor_state == ipc::State::PRESSED ? logik::Event::RESET_PRESSED : logik::Event::RESET_RELEASED);

        if (m_partnerCoid != -1) {
            MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_MSG, payload);
        }
    }

    // 2. LICHTSCHRANKEN & SENSOREN (BLOCKED / CLEARED)
    else if (sensor_id == ipc::LS_ENTRY) {
        std::cout << "[Dispatcher] LS_ENTRY: "
                  << (sensor_state == ipc::State::BLOCKED ? "BLOCKED" : "CLEARED") << std::endl;
        m_logik->processEvent(sensor_state == ipc::State::BLOCKED ? logik::Event::ENTRY_BLOCKED : logik::Event::ENTRY_CLEARED);
    }
    else if (sensor_id == ipc::HEIGHT) {
        if (sensor_state == ipc::State::BLOCKED) {
            std::cout << "[Dispatcher] HEIGHT: BLOCKED" << std::endl;
            m_logik->processEvent(logik::Event::HEIGHT_BLOCKED);
        }
        else if (sensor_state == ipc::State::CLEARED) {
            std::cout << "[Dispatcher] HEIGHT: CLEARED" << std::endl;
            m_logik->processEvent(logik::Event::HEIGHT_CLEARED);
        }
        else if (sensor_state == ipc::State::HIGH_ERROR) {
            std::cout << "[Dispatcher] HEIGHT: HIGH_ERROR" << std::endl;
            m_logik->processEvent(logik::Event::HEIGHT_ERROR);
        }
        else {
            std::cout << "[Dispatcher] HEIGHT: Typ=" << sensor_state << std::endl;
            m_logik->workpieceTypeFromHS(static_cast<WpType>(sensor_state));
        }
    }
    else if (sensor_id == ipc::SENSOR_METAL) {
        std::cout << "[Dispatcher] METAL: "
                  << (sensor_state == ipc::State::BLOCKED ? "DETECTED" : "CLEARED") << std::endl;
        m_logik->processEvent(sensor_state == ipc::State::BLOCKED ? logik::Event::METAL_DETECTED : logik::Event::METAL_CLEARED);
    }
    else if (sensor_id == ipc::LS_SWITCH) {
        std::cout << "[Dispatcher] LS_SWITCH: "
                  << (sensor_state == ipc::State::BLOCKED ? "BLOCKED" : "CLEARED") << std::endl;
        m_logik->processEvent(sensor_state == ipc::State::BLOCKED ? logik::Event::EJECTOR_BLOCKED : logik::Event::EJECTOR_CLEARED);
    }
    else if (sensor_id == ipc::LS_END) {
        std::cout << "[Dispatcher] LS_END: "
                  << (sensor_state == ipc::State::BLOCKED ? "BLOCKED" : "CLEARED") << std::endl;
        m_logik->processEvent(sensor_state == ipc::State::BLOCKED ? logik::Event::END_BLOCKED : logik::Event::END_CLEARED);
    }
    else if (sensor_id == ipc::LS_CHUTE) {
        std::cout << "[Dispatcher] LS_CHUTE: "
                  << (sensor_state == ipc::State::BLOCKED ? "BLOCKED" : "CLEARED") << std::endl;
        m_logik->processEvent(sensor_state == ipc::State::BLOCKED ? logik::Event::CHUTE_BLOCKED : logik::Event::CHUTE_CLEARED);
    }
    else {
        std::cout << "[Dispatcher] Unbekannter HAL-Sensor id=" << sensor_id
                  << " state=" << sensor_state << std::endl;
    }
}

// ============================================================================
// EVENT HANDLER NETZWERK
// ============================================================================
void Dispatcher::handleNetworkPulse(uint32_t payload) {
    uint16_t sensor_id = ipc::decodeSensorID(payload);
    uint16_t message_type = ipc::decodeSensorState(payload);

    if (sensor_id == ipc::BTN_START) {
        std::cout << "[Dispatcher] Partner-START: "
                  << (message_type == ipc::State::PRESSED ? "PRESSED" : "RELEASED") << std::endl;
        m_logik->processEvent(message_type == ipc::State::PRESSED ? logik::Event::PARTNER_START_PRESSED : logik::Event::PARTNER_START_RELEASED);
    }
    else if (sensor_id == ipc::BTN_RESET) {
        std::cout << "[Dispatcher] Partner-RESET: "
                  << (message_type == ipc::State::PRESSED ? "PRESSED" : "RELEASED") << std::endl;
        m_logik->processEvent(message_type == ipc::State::PRESSED ? logik::Event::PARTNER_RESET_PRESSED : logik::Event::PARTNER_RESET_RELEASED);
    }
    else if (sensor_id == ipc::NetworkMessage::REQ_RECEIVE) {
        std::cout << "[Dispatcher] Netzwerk: REQ_TO_GIVE_WP" << std::endl;
        m_logik->processEvent(logik::Event::REQ_TO_GIVE_WP);
    }
    else if (sensor_id == ipc::NetworkMessage::READY_RECEIVE) {
        std::cout << "[Dispatcher] Netzwerk: READY_TO_RECEIVE_WP" << std::endl;
        m_logik->processEvent(logik::Event::READY_TO_RECEIVE_WP);
    }
    else if (sensor_id == ipc::NetworkMessage::CHUTE){
        std::cout << "[Dispatcher] Partner-Rutsche: "
                  << (message_type == ipc::VOLL ? "VOLL" : "LEER") << std::endl;
        bool isFull = (message_type == ipc::VOLL);
        if (m_logik != nullptr) {
            m_logik->setPartnerChuteStatus(isFull);
        }

    }
    else if (message_type == ipc::NetworkMessage::ESTOP_STATUS_QUERY) {
        std::cout << "[Dispatcher] Partner fragt ESTOP-Status an -> antworte." << std::endl;
        sendLocalEstopStatusToPartner();
    }
    else {
        std::cout << "[Dispatcher] Unbekannte Partner-Nachricht id=" << sensor_id
                  << " state=" << message_type << std::endl;
    }
}

// ============================================================================
// DURCHREICHE-METHODEN FÜR DAS NETZWERK
// ============================================================================
void Dispatcher::sendWorkpieceInfo(int id, WpType type) {
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
            std::cout << "[Dispatcher] ESTOP: PRESSED (Active-Low)" << std::endl;
            m_logik->processEvent(logik::Event::ESTOP_PRESSED);

            if (m_partnerCoid != -1) {
                uint32_t netPayload = ipc::encodeHalPayload(ipc::BTN_ESTOP, ipc::State::RELEASED);
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_ESTOP, netPayload);
            }
        }
        else if (sensor_state == ipc::State::PRESSED) {
            std::cout << "[Dispatcher] ESTOP: RELEASED (Active-Low)" << std::endl;
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
        m_partnerEstopPressed = true;
        std::cout << "[Dispatcher] Partner-ESTOP: PRESSED" << std::endl;
        m_logik->processEvent(logik::Event::PARTNER_ESTOP_PRESSED);
    }
    else if (partner_state == ipc::State::PRESSED) {
        m_partnerEstopPressed = false;
        std::cout << "[Dispatcher] Partner-ESTOP: RELEASED" << std::endl;
        m_logik->processEvent(logik::Event::PARTNER_ESTOP_RELEASED);
    }
}

// ============================================================================
// Calibration
// ============================================================================
void Dispatcher::handleCalibrationPulse(uint32_t payload) {
    using CM = ipc::CalibMode;

    uint16_t rawValue = 0;
    uint16_t calibMode = 0;
    ipc::decodeCalibPayload(payload, rawValue, calibMode);

    switch (static_cast<CM>(calibMode)) {

        case CM::EMPTY_BELT:
            s_liveEmptyBelt = rawValue;
        break;

        case CM::FLAT_WP:
            s_liveFlatCenter = rawValue;
        break;

        case CM::MIN_SAMPLES:
            s_liveMinSamples = static_cast<uint32_t>(rawValue);
        if (m_logik != nullptr) {
            m_logik->setLiveAdcCalibration(s_liveEmptyBelt, s_liveFlatCenter, s_liveMinSamples);
        }
        break;

        default:
            break;
    }
}

/**
 * @brief Prueft über den CalibrationManager, ob die Textdatei mit den
 * gelernten Ticks und ADC-Fenstern existiert und gueltige Werte enthaelt.
 */
bool Dispatcher::requestFileExists() {
    return m_calibrationManager && m_calibrationManager->fileExists();
}

bool Dispatcher::isEstopPressed() {
    if (m_sensors == nullptr) {
        return false;
    }
    return m_sensors->isEstopPressed();
}

bool Dispatcher::isPartnerEstopPressed() const {
    return m_partnerEstopPressed.load();
}

void Dispatcher::sendeChuteStats(bool isVoll) {
    if (m_partnerCoid == -1) {return;}
    ipc::State state = isVoll ? ipc::VOLL : ipc::EMPTY;
    uint32_t payload = ipc::encodeHalPayload(ipc::NetworkMessage::CHUTE, state);
    MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_MSG, payload);
}


void Dispatcher::sendLocalEstopStatusToPartner() {
    if (m_partnerCoid == -1) {
        return;
    }

    const bool pressed = isEstopPressed();
    const ipc::State state = pressed ? ipc::State::RELEASED : ipc::State::PRESSED;
    const uint32_t payload = ipc::encodeHalPayload(ipc::BTN_ESTOP, state);
    MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_ESTOP, payload);
    std::cout << "[Dispatcher] ESTOP-Status an Partner gesendet: "
              << (pressed ? "PRESSED" : "RELEASED") << std::endl;
}

void Dispatcher::requestPartnerEstopStatus() {
    if (m_partnerCoid == -1) {
        return;
    }

    const uint32_t payload = ipc::encodeHalPayload(0, ipc::NetworkMessage::ESTOP_STATUS_QUERY);
    MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::PARTNER_MSG, payload);
    std::cout << "[Dispatcher] Partner-ESTOP-Status angefragt." << std::endl;
}

void Dispatcher::forwardCalibrationToHal(std::uint16_t emptyBelt, std::uint16_t flatCenter, std::uint32_t minSamples) {
    m_sensors->updateCalibrationThresholds(static_cast<uint16_t>(ipc::CalibMode::EMPTY_BELT), emptyBelt);
    m_sensors->updateCalibrationThresholds(static_cast<uint16_t>(ipc::CalibMode::FLAT_WP), flatCenter);
    m_sensors->updateCalibrationThresholds(static_cast<uint16_t>(ipc::CalibMode::MIN_SAMPLES),
                                           static_cast<uint16_t>(minSamples));
}
