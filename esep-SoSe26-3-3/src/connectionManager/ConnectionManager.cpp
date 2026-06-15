/**
 * @file ConnectionManager.cpp
 * @brief Implementierung des QNX-Verbindungsmanagements und der Kontaktueberwachung.
 */

#include "ConnectionManager.hpp"
#include <iostream>
#include <chrono>
#include <cerrno>
#include <sys/neutrino.h>
#include <sys/netmgr.h>
#include <string>
#include <unistd.h>

ConnectionManager::ConnectionManager()
    : m_localCoid(-1),
      m_partnerCoid(-1),
      m_pos(AnlagenPosition::UNKNOWN_POSITION),
      m_partnerAlive(false),
      m_isConnected(false),
      m_connectGraceTicks(0),
      m_missedHeartbeats(0),
      m_isSearching(false),
      m_heartbeatRunning(false)
{
}

ConnectionManager::~ConnectionManager() {
    m_isSearching = false;
    stopHeartbeatSender();
    if (m_connectThread.joinable()) {
        m_connectThread.join();
    }

    if (m_partnerCoid != -1) {
        ConnectDetach(m_partnerCoid);
    }
}

void ConnectionManager::init(int localCoid, AnlagenPosition pos) {
    m_localCoid = localCoid;
    m_pos = pos;

    std::cout << "[ConnMgr] Initialisiere als Anlage "
              << (m_pos == AnlagenPosition::VORN_ANLAGE_1 ? "1" : "2") << std::endl;

    if (m_pos == AnlagenPosition::HINTEN_ANLAGE_2) {
        std::cout << "[ConnMgr] Ich bin Anlage 2. Starte sofort die aktive Suche..." << std::endl;
        connectToPartner();
    } else {
        std::cout << "[ConnMgr] Ich bin Anlage 1. Warte passiv im Hintergrund." << std::endl;
    }
}

void ConnectionManager::connectToPartner() {
    if (m_isSearching || m_isConnected) {
        return;
    }

    if (m_connectThread.joinable()) {
        m_connectThread.join();
    }

    m_isSearching = true;
    m_connectThread = std::thread(&ConnectionManager::connectionTask, this);
}

void ConnectionManager::scheduleReconnectAsync() {
    if (m_localCoid != -1) {
        MsgSendPulse(m_localCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::RECONNECT_MSG, 0);
    }
}

void ConnectionManager::scheduleReconnect() {
    if (m_partnerCoid != -1) {
        ConnectDetach(m_partnerCoid);
        m_partnerCoid = -1;
    }

    m_isSearching = false;
    if (m_connectThread.joinable()) {
        m_connectThread.join();
    }

    std::cout << "[ConnMgr] Starte erneute Partnersuche..." << std::endl;
    connectToPartner();
}

void ConnectionManager::connectionTask() {
    std::string targetName = (m_pos == AnlagenPosition::VORN_ANLAGE_1) ? "Anlage_2" : "Anlage_1";
    std::string fullPath = "/dev/name/global/" + targetName;

    std::cout << "[ConnMgr] Beginne Suche nach Partner '" << targetName << "' im Hintergrund..." << std::endl;

    // Evtl. noch laufenden (oder auf totem qnet-Link blockierten) Sender-Thread
    // sauber beenden, bevor eine neue Verbindung aufgebaut wird. Das Warten
    // passiert hier im Hintergrund-Thread, nie auf dem Dispatcher-Thread.
    stopHeartbeatSender();

    while (m_isSearching) {
        if (access(fullPath.c_str(), F_OK) != -1) {
            std::cout << "........................" << std::flush;
            int coid = name_open(targetName.c_str(), NAME_FLAG_ATTACH_GLOBAL);

            if (coid != -1) {
                m_partnerCoid = coid;
                m_partnerAlive = true;
                m_isConnected = true;
                m_connectGraceTicks = kConnectGraceTicks;
                m_missedHeartbeats = 0;

                std::cout << "\n[ConnMgr] PARTNER GEFUNDEN! Verbindung steht. (Partner-COID: " << coid << ")" << std::endl;

                if (m_localCoid != -1) {
                    MsgSendPulse(m_localCoid, 1, ipc::CONNECTION_MSG, coid);
                }

                startHeartbeatSender();

                m_isSearching = false;
                break;
            }
        } else {
            std::cout << "." << std::flush;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void ConnectionManager::notifyLocalDisconnect() {
    if (m_localCoid != -1) {
        std::cerr << "[ConnMgr] Sende DISCONNECT-Puls an Dispatcher." << std::endl;
        MsgSendPulse(m_localCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::DISCONNECT, 0);
    }
}

void ConnectionManager::startHeartbeatSender() {
    // Vorherigen (beendeten) Sender-Thread aufraeumen.
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }
    m_heartbeatRunning = true;
    m_heartbeatThread = std::thread(&ConnectionManager::heartbeatSenderTask, this);
}

void ConnectionManager::stopHeartbeatSender() {
    m_heartbeatRunning = false;
    // Niemals den eigenen Thread joinen (forceDisconnect kann aus dem
    // Sender-Thread heraus aufgerufen werden).
    if (m_heartbeatThread.joinable() &&
        m_heartbeatThread.get_id() != std::this_thread::get_id()) {
        m_heartbeatThread.join();
    }
}

void ConnectionManager::heartbeatSenderTask() {
    while (m_heartbeatRunning && m_isConnected) {
        int coid = m_partnerCoid;
        if (coid != -1) {
            // Dieser Send kann ueber einen toten qnet-Link blockieren -
            // aber nur DIESER Thread, nicht der Dispatcher/Watchdog.
            if (MsgSendPulse(coid, SIGEV_PULSE_PRIO_INHERIT, ipc::HEARTBEAT, 0) == -1) {
                std::cerr << "[ConnMgr] Heartbeat-Send fehlgeschlagen (errno="
                          << errno << ") -> Verbindung trennen." << std::endl;
                forceDisconnect();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kHeartbeatSendIntervalMs));
    }
}

void ConnectionManager::acknowledgeConnectionLost() {
    m_isConnected = false;
    m_partnerAlive = false;
    // Nur Flag setzen - kein join() hier, da diese Methode auf dem
    // Dispatcher-Thread laeuft und der Sender ggf. noch im qnet-Send haengt.
    m_heartbeatRunning = false;
    scheduleReconnectAsync();
}

void ConnectionManager::forceDisconnect() {
    if (!m_isConnected.exchange(false)) {
        return;
    }

    m_partnerAlive = false;
    m_heartbeatRunning = false;
    std::cerr << "[ConnMgr] Verbindung erzwungen getrennt -> DISCONNECT an Dispatcher." << std::endl;
    notifyLocalDisconnect();
}

void ConnectionManager::performReconnect() {
    scheduleReconnect();
}

void ConnectionManager::heartbeatTackt() {
    if (!m_isConnected) {
        return;
    }

    if (m_connectGraceTicks > 0) {
        --m_connectGraceTicks;
        m_partnerAlive = true;
        m_missedHeartbeats = 0;
        return;
    }

    // Wurde seit dem letzten Tick mindestens ein HEARTBEAT empfangen?
    if (m_partnerAlive.exchange(false)) {
        m_missedHeartbeats = 0;
        return;
    }

    // Kein HEARTBEAT in diesem Fenster -> erst nach mehreren verpassten
    // Ticks trennen (robust gegen Sender/Watchdog-Drift).
    if (++m_missedHeartbeats >= kMaxMissedHeartbeats) {
        m_isConnected = false;
        m_missedHeartbeats = 0;
        std::cerr << "[ConnMgr] Partner-Heartbeat ausgeblieben (" << kMaxMissedHeartbeats
                  << " Ticks) -> DISCONNECT an Dispatcher." << std::endl;
        notifyLocalDisconnect();
    }
}

void ConnectionManager::partnerAlive() {
    m_partnerAlive = true;
    m_missedHeartbeats = 0;
}

bool ConnectionManager::isConnected() const {
    return m_isConnected;
}

int ConnectionManager::getPartnerCoid() const {
    return m_partnerCoid;
}
