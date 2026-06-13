/**
 * @file ConnectionManager.cpp
 * @brief Implementierung des QNX-Verbindungsmanagements und der Kontaktueberwachung.
 */

#include "ConnectionManager.hpp"
#include <iostream>
#include <chrono>
#include <sys/neutrino.h>
#include <sys/netmgr.h>
#include <string>
#include <unistd.h>

// ============================================================================
// Konstruktor & Destruktor
// ============================================================================

ConnectionManager::ConnectionManager()
    : m_localCoid(-1),
      m_partnerCoid(-1),
      m_pos(AnlagenPosition::UNKNOWN_POSITION),
      m_partner_Alive(false),
      m_isConnected(false),
      m_connectGraceTicks(0),
      m_isSearching(false)
{
}

ConnectionManager::~ConnectionManager() {
    m_isSearching = false;
    if (m_connectThread.joinable()) {
        m_connectThread.join();
    }

    if (m_partnerCoid != -1) {
        ConnectDetach(m_partnerCoid);
    }
}

// ============================================================================
// Initialisierung (Control Plane)
// ============================================================================

void ConnectionManager::init(int localCoid,AnlagenPosition pos) {
    m_localCoid = localCoid;
    m_pos = pos;

    std::cout << "[ConnMgr] Initialisiere als Anlage "
              << (m_pos == AnlagenPosition::VORN_ANLAGE_1 ? "1" : "2") << std::endl;

    // GNS-Name wird ausschliesslich vom Dispatcher registriert (kein doppeltes name_attach).

    if (m_pos == AnlagenPosition::HINTEN_ANLAGE_2) {
        std::cout << "[ConnMgr] Ich bin Anlage 2. Starte sofort die aktive Suche..." << std::endl;
        connectToPartner(); // Startet den Thread
    } else {
        std::cout << "[ConnMgr] Ich bin Anlage 1. Warte passiv im Hintergrund." << std::endl;
        // Ende der Methode. Thread wird nicht gestartet!
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

// ============================================================================
// Der Hintergrund-Thread (Lazy Loading)
// ============================================================================

void ConnectionManager::connectionTask() {
    // Wen suchen wir?
    std::string targetName = (m_pos == AnlagenPosition::VORN_ANLAGE_1) ? "Anlage_2" : "Anlage_1";

    // NEU: Wir bauen den echten QNX-Pfad zusammen
    std::string fullPath = "/dev/name/global/" + targetName;

    std::cout << "[ConnMgr] Beginne Suche nach Partner '" << targetName << "' im Hintergrund..." << std::endl;

    while (m_isSearching) {

        // ====================================================================
        // PROFI-TRICK: Checken, ob der Briefkasten ueberhaupt existiert!
        // F_OK prueft, ob die Datei im Netzwerk vorhanden ist.
        // ====================================================================
        if (access(fullPath.c_str(), F_OK) != -1) {
            std::cout << "........................" << std::flush;
            // Datei ist da! Jetzt ist name_open zu 100% sicher und friert nicht ein.
            int coid = name_open(targetName.c_str(), NAME_FLAG_ATTACH_GLOBAL);

            if (coid != -1) {
                // PARTNER GEFUNDEN!
                m_partnerCoid = coid;
                m_partner_Alive = true;
                m_isConnected = true;
                m_connectGraceTicks = 10; // 10 x 50ms = 500ms Kulanz nach Verbindungsaufbau

                std::cout << "\n[ConnMgr] PARTNER GEFUNDEN! Verbindung steht. (Partner-COID: " << coid << ")" << std::endl;

                // Dispatcher benachrichtigen
                if (m_localCoid != -1) {
                    MsgSendPulse(m_localCoid, 1, ipc::CONNECTION_MSG, coid);
                }

                m_isSearching = false; // Suche beenden
                break;
            }
        } else {
            // Die Datei gibt es noch nicht. Anlage 1 ist also noch aus.
            // Wir drucken einen Punkt und warten.
            std::cout << "." << std::flush;
        }

        // 50ms warten – schnellerer Verbindungsaufbau fuer Heartbeat-Synchronisation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// ============================================================================
// Hard-Realtime Kontaktueberwachung (Watchdog)
// ============================================================================

void ConnectionManager::acknowledgeConnectionLost() {
    if (!m_isConnected.exchange(false)) {
        return;
    }

    scheduleReconnectAsync();
}

void ConnectionManager::triggerConnectionLost(const char* reason) {
    if (!m_isConnected.exchange(false)) {
        return;
    }

    std::cerr << "[ConnMgr] " << reason << std::endl;

    const int partnerCoid = m_partnerCoid.load();
    if (partnerCoid != -1) {
        MsgSendPulse(partnerCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::DISCONNECT, 0);
    }

    scheduleReconnectAsync();
}

void ConnectionManager::forceDisconnect() {
    triggerConnectionLost("Verbindung erzwungen getrennt.");
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
        m_partner_Alive = true;
        return;
    }

    if (m_partner_Alive == false) {
        triggerConnectionLost("Heartbeat-Deadline verpasst – Verbindung verloren.");
    } else {
        m_partner_Alive = false;
    }
}

void ConnectionManager::partnerAlive() {
    // Der Dispatcher ruft dies auf, weil er gerade einen Puls (Ping oder Daten) vom Partner empfangen hat.
    // Wir markieren den Partner als lebendig, um dem naechsten heartbeatTackt() zu entkommen.
    m_partner_Alive = true;
}

// ============================================================================
// Getter fuer den Dispatcher
// ============================================================================

bool ConnectionManager::isConnected() const {
    return m_isConnected;
}

int ConnectionManager::getPartnerCoid() const {
    return m_partnerCoid;
}
