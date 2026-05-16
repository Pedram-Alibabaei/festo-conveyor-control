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
      m_connection(nullptr),
      m_attachHandle(nullptr) {
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

void Dispatcher::init(AnlagenPosition pos) {
    m_position = pos;

    std::string myName = (pos == VORN_ANLAGE_1) ? "Anlage_1" : "Anlage_2";



    m_attachHandle = name_attach(NULL, myName.c_str(), NAME_FLAG_ATTACH_GLOBAL);
    if (m_attachHandle == nullptr) {
        std::cerr << "[Dispatcher] Fehler: name_attach fehlgeschlagen!" << std::endl;
        return;
    }

    // Die Channel-ID wird von name_attach generiert!
    m_chid = m_attachHandle->chid;

    // 3. Verbindung zu sich selbst aufbauen
    m_localCoid = ConnectAttach(0, 0, m_chid, _NTO_SIDE_CHANNEL, 0);

    std::cout << "[Dispatcher] Bereit auf Kanal " << m_chid
              << " als '" << myName << "' (Lokale COID: " << m_localCoid << ")" << std::endl;
}

void Dispatcher::stop() {
    m_running = false;
}

// ============================================================================
// Hauptschleife (Der Reaktor)
// ============================================================================

void Dispatcher::run() {
    // Wir brauchen einen flexiblen Buffer, der Pulse UND Systemnachrichten fassen kann
    union {
        struct _pulse pulse;
        uint16_t type; // QNX Nachrichten fangen immer mit dem Typ an
        uint8_t msg_buffer[256];
    } msg;

    m_running = true;
    std::cout << "[Dispatcher] Reaktor-Schleife gestartet..." << std::endl;

    while (m_running) {
        // NEU: MsgReceive blockiert und empfaengt ALLES (Pulse und Nachrichten)
        int rcvid = MsgReceive(m_chid, &msg, sizeof(msg), NULL);

        if (rcvid == -1) {
            if (errno == EINTR) continue;
            break;
        }

        if (rcvid == 0) {
            // Es ist ein Puls! (rcvid ist 0) -> An deine gewohnte Logik weiterleiten
            handlePulse(msg.pulse);
        } else {
            // ================================================================
            // EINE ECHTE NACHRICHT! Das ist das name_open() der anderen Anlage!
            // ================================================================
            if (msg.type == _IO_CONNECT) {
                // Wir MUESSEN QNX antworten, sonst friert der Such-Thread von Anlage 2 ein!
                MsgReply(rcvid, EOK, NULL, 0);
                std::cout << "[Dispatcher] name_open (Handshake) vom Partner akzeptiert!" << std::endl;
            } else {
                // Andere System-Nachrichten lehnen wir einfach ab
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

        // ====================================================================
        // LOKALE HARDWARE & SYSTEM
        // ====================================================================
        case ipc::HAL_INPUT:
            handleHalPulse(pulse.value.sival_int);
            break;

        case ipc::CONNECTION_MSG:
            // ConnectionManager hat den Partner gefunden
            handleConnectionPulse(pulse.value.sival_int);
            break;

        case ipc::DISCONNECT:
            // KRITISCH: Verbindung verloren oder Not-Halt
            std::cerr << "[Dispatcher] ACHTUNG: Verbindung unterbrochen! Not-Halt." << std::endl;
            if (m_actuators) m_actuators->driveStop();
            // Hier spaeter: if (m_logik) m_logik->geheInFehlerzustand();
            break;

            // ====================================================================
                    // TIMER EVENTS
                    // ====================================================================
                    case ipc::BELT_TICK: {
                        // TEST: Gruene Ampel bei jedem Takt umschalten (Toggle)
                        static bool toggleGreen = false;
                        toggleGreen = !toggleGreen;
                        if (m_actuators) {
                            if (toggleGreen) m_actuators->lightGreenOn();
                            else m_actuators->lightGreenOff();
                        }

                        handleTimerPulse(pulse.value.sival_int);
                        break;
                    }

                    case ipc::HEARTBEATTACKT: {
                        // TEST: Gelbe Ampel bei jedem Takt umschalten
                        static bool toggleYellow = false;
                        toggleYellow = !toggleYellow;
                        if (m_actuators) {
                            if (toggleYellow) m_actuators->lightYellowOn();
                            else m_actuators->lightYellowOff();
                        }

                        // Der Watchdog-Timer sagt: Deadline pruefen!
                        if (m_connection) {
                            m_connection->heartbeatTackt();

                            // Falls verbunden: Heartbeat an den Partner senden
                            if (m_connection->isConnected()) {
                                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::HEARTBEAT, 0);
                            }
                        }
                        break;
                    }

                    // ====================================================================
                    // NETZWERK
                    // ====================================================================
                    case ipc::HEARTBEAT: {
                        // TEST: Rote Ampel bei jeder empfangenen Nachricht umschalten
                        static bool toggleRed = false;
                        toggleRed = !toggleRed;
                        if (m_actuators) {
                            if (toggleRed) m_actuators->lightRedOn();
                            else m_actuators->lightRedOff();
                        }

                        if (m_connection && m_connection->isConnected()){
                            m_connection->partnerAlive();
                        }
                        break;
                    }


        case ipc::SORTER_TIMER_TICK:
            // Der One-Shot-Timer der Weiche ist abgelaufen!
            std::cout << "[Dispatcher] Sorter-Zeit abgelaufen! Schliesse/Fahre automatisch ein." << std::endl;
            if (m_actuators) m_actuators->closeJunction();
            break;

        // ====================================================================
        // NETZWERK BASIS
        // ====================================================================
        case ipc::NETWORK_MSG:
            // Der Partner hat uns etwas geschickt (Ping oder Daten)
            if (m_connection) m_connection->partnerAlive(); // Watchdog zuruecksetzen
            handleNetworkPulse(pulse.value.sival_int);
            break;

        case ipc::ANLAGE_2_CIID:
            // Diesen Puls empfängt NUR Anlage 1 über das Netzwerk!
            std::cout << "[Dispatcher] Handshake von Anlage 2 empfangen! Starte eigene Suche..." << std::endl;
            if (m_connection && !m_connection->isConnected()) {
                m_connection->connectToPartner(); // Anlage 1 macht jetzt name_open
            }
            break;

        // ====================================================================
        // NETZWERK WERKSTÜCK-ÜBERGABE
        // ====================================================================
        case ipc::WP_INFO: {
            // WICHTIG: Die {} Klammern sind noetig, weil wir hier Variablen deklarieren!
            // Entpacken: ID in den oberen 16 Bit, Typ in den unteren 16 Bit
            int id = (pulse.value.sival_int >> 16) & 0xFFFF;
            WpType type = static_cast<WpType>(pulse.value.sival_int & 0xFFFF);

            std::cout << "[Dispatcher] Netz-Nachricht: Werkstueck (ID: " << id << ", Typ: " << type << ") im Anflug!" << std::endl;
            // Hier spaeter: if (m_logik) m_logik->empfangeWpInfo(id, type);
            break;
        }

        case ipc::REQ_RECEIVE:
            std::cout << "[Dispatcher] Netz-Nachricht: Partner fragt an -> Darf ich uebergeben?" << std::endl;
            // Hier spaeter: if (m_logik) m_logik->pruefeObUebergabeMoeglich();
            break;

        case ipc::READY_RECEIVE:
            std::cout << "[Dispatcher] Netz-Nachricht: Partner meldet -> Bin bereit, lass kommen!" << std::endl;
            // Hier spaeter: if (m_logik) m_logik->uebergabeStarten();
            break;

        case ipc::UPDATE_LIST: {
            WpType type = static_cast<WpType>(pulse.value.sival_int);
            std::cout << "[Dispatcher] Netz-Nachricht: Partner bittet um Update der Liste (Typ: " << type << ")." << std::endl;
            // Hier spaeter: if (m_logik) m_logik->updateWpListe(type);
            break;
        }

        // ====================================================================
        // FALLBACK
        // ====================================================================
        default:
            std::cout << "[Dispatcher] Unbekannter Puls-Code: " << (int)pulse.code << std::endl;
            break;
    }
}


void Dispatcher::handleConnectionPulse(uint32_t payload) {
    m_partnerCoid = payload; // Die COID vom ConnectionManager speichern
    std::cout << "[Dispatcher] Partner-Verbindung aktiv (COID: " << m_partnerCoid << ")" << std::endl;

    // Asymmetrischer Start: Anlage 2 sagt Anlage 1 "Hallo"
    if (m_position == HINTEN_ANLAGE_2) {
        std::cout << "[Dispatcher] Sende Initial-Handshake an Anlage 1..." << std::endl;
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::ANLAGE_2_CIID, 1);
    }
}

// ============================================================================
// EVENT HANDLER LOKAL (HAL)
// ============================================================================
void Dispatcher::handleHalPulse(uint32_t payload) {
    uint16_t sensor_id = ipc::decodeSensorID(payload);
    uint16_t sensor_state = ipc::decodeSensorState(payload);

    if (!m_actuators) return;

    // ========================================================================
    // TASTER & SCHALTER
    // ========================================================================

    // TASTE 1: START
    if (sensor_id == ipc::BTN_START) {
        if (sensor_state == 1) {
            std::cout << ">>> TEST: Start gedrueckt! Gruen blinkt! <<<" << std::endl;
            lightGreenBlinkFast();

            // Nachricht an Partner senden, falls verbunden
            if (m_partnerCoid != -1) {
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::NETWORK_MSG, payload);
                std::cout << "[" << getAnlagenPositionName() << "] Info an Partner gesendet!" << std::endl;
            } else {
                std::cout << "[" << getAnlagenPositionName() << "] Konnte Info nicht senden. Partner ist offline." << std::endl;
            }
        }
        else {
            std::cout << ">>> TEST: Start losgelassen! Gruen aus! <<<" << std::endl;
            lightGreenOff();

            // Nachricht an Partner senden, falls verbunden
            if (m_partnerCoid != -1) {
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::NETWORK_MSG, payload);
            }
        }
    }
    // TASTE 2: RESET
    else if (sensor_id == ipc::BTN_RESET) {
        if (sensor_state == 1) {
            std::cout << ">>> TEST: Reset gedrueckt! Gelb blinkt! <<<" << std::endl;
            lightYellowBlinkFast();
        }
        else {
            std::cout << ">>> TEST: Reset losgelassen! Gelb aus! <<<" << std::endl;
            lightYellowOff();
        }
    }
    // TASTE 3: STOP
    else if (sensor_id == ipc::BTN_STOP) {
        if (sensor_state == 0) { // Low-aktiv
            std::cout << ">>> TEST: Stop gedrueckt! Rot blinkt! <<<" << std::endl;
            m_actuators->lightRedBlinkSlow();
            driveStop(); // Anlage stoppen
        }
        else {
            std::cout << ">>> TEST: Stop losgelassen! Rot aus! <<<" << std::endl;
            lightRedOff();
        }
    }
    // E-STOP (Not-Halt) - WICHTIG!
    else if (sensor_id == ipc::BTN_ESTOP) {
        if (sensor_state == 0) { // Not-Halt ist fast immer Low-aktiv (0 = gedrückt)
            std::cout << ">>> TEST: NOT-HALT GEDRUECKT! ALLES STOPPT! <<<" << std::endl;
            m_actuators->turnAllOff();
            m_actuators->driveStop();
            m_actuators->lightRedBlinkFast();

            // Unbedingt den Partner in den Not-Halt reissen!
            if (m_partnerCoid != -1) {
                MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::DISCONNECT, 0);
            }
        }
        else {
            std::cout << ">>> TEST: Not-Halt entriegelt! <<<" << std::endl;
            m_actuators->lightRedOff();
        }
    }

    // ========================================================================
    // LICHTSCHRANKEN & SENSOREN
    // ========================================================================

    // LICHTSCHRANKE 1: Einlauf (Entry)
    else if (sensor_id == ipc::LS_ENTRY) {
        if (sensor_state == 0) {
            std::cout << ">>> TEST: Werkstueck am Einlauf erkannt! Motor faehrt RECHTS! <<<" << std::endl;
            m_actuators->driveRight();
            m_actuators->lightGreenBlinkSlow();
        }
    }
    // LICHTSCHRANKE 2: Hoehenmessung
    else if (sensor_id == ipc::LS_HEIGHT) { // Evtl. heisst es bei dir SENSOR_HEIGHT
        if (sensor_state == 0) {
            std::cout << ">>> TEST: Werkstueck unter der Hoehenmessung! <<<" << std::endl;
        }
    }
    // SENSOR: Metallerkennung (Induktiv)
    else if (sensor_id == ipc::SENSOR_METAL) {
        if (sensor_state == 1) { // Metallsensor ist meist High-aktiv (1 = Metall)
            std::cout << ">>> TEST: Metall-Werkstueck erkannt! <<<" << std::endl;
        } else {
            std::cout << ">>> TEST: Kein Metall mehr unterm Sensor. <<<" << std::endl;
        }
    }
    // LICHTSCHRANKE 3: Weiche (Switch/Junction)
    else if (sensor_id == ipc::LS_SWITCH) { // Evtl. heisst es bei dir LS_JUNCTION
        if (sensor_state == 0) {
            std::cout << ">>> TEST: Werkstueck passiert die Weiche! <<<" << std::endl;
        }
    }
    // LICHTSCHRANKE 4: Auslauf (End)
    else if (sensor_id == ipc::LS_END) {
        if (sensor_state == 0) {
            std::cout << ">>> TEST: Werkstueck am Ende angekommen! Motor STOPP! <<<" << std::endl;
            m_actuators->driveStop();
            m_actuators->lightGreenOff();
        }
    }
    // LICHTSCHRANKE 5: Rutsche (Chute)
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


// ============================================================================
// DURCHREICHE-METHODEN FÜR DAS NETZWERK
// ============================================================================

void Dispatcher::workpieceInfo(int id, WpType type) {
    if (m_partnerCoid != -1) {
        // TRICK: Wir packen die ID in die oberen 16 Bit und den Typ in die unteren 16 Bit
        uint32_t payload = (static_cast<uint32_t>(id) << 16) | static_cast<uint32_t>(type);
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::WP_INFO, payload);
        std::cout << "[Dispatcher] workpieceInfo an Partner gesendet (ID: " << id << ")!" << std::endl;
    } else {
        std::cout << "[Dispatcher] Fehler: workpieceInfo nicht gesendet (Partner offline)." << std::endl;
    }
}

void Dispatcher::RequestToReceiveWp() {
    if (m_partnerCoid != -1) {
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT,ipc::NETWORK_MSG, ipc::MSG_REQ_RECEIVE);
        std::cout << "[Dispatcher] RequestToReceiveWp an Partner gesendet!" << std::endl;
    }
}

void Dispatcher::readyToReceiveWp() {
    if (m_partnerCoid != -1) {
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT,ipc::NETWORK_MSG, ipc::MSG_READY_RECEIVE);
        std::cout << "[Dispatcher] readyToReceiveWp an Partner gesendet!" << std::endl;
    }
}

void Dispatcher::updateWpListe(WpType type) {
    if (m_partnerCoid != -1) {
        MsgSendPulse(m_partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::NETWORK_MSG, static_cast<uint32_t>(type));
        std::cout << "[Dispatcher] updateWpListe an Partner gesendet!" << std::endl;
    }
}

void Dispatcher::startBeltClock(int ms) {

}
