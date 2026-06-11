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
    // Sicherheits-Check: Nur starten, wenn wir nicht schon suchen oder verbunden sind
    if (!m_isSearching && !m_isConnected) {
        m_isSearching = true;
        m_connectThread = std::thread(&ConnectionManager::connectionTask, this);
    }
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
                m_connectGraceTicks = 40; // 40 x 50ms = 2s Kulanz nach Verbindungsaufbau

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

        // 100ms warten – schnellerer Verbindungsaufbau fuer Heartbeat-Synchronisation
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ============================================================================
// Hard-Realtime Kontaktueberwachung (Watchdog)
// ============================================================================

void ConnectionManager::heartbeatTackt() {
    // Wenn gar keine Verbindung besteht, muessen wir auch nichts ueberwachen.
    if (!m_isConnected) {
        return;
    }

    if (m_connectGraceTicks > 0) {
        --m_connectGraceTicks;
        m_partner_Alive = true;
        return;
    }

    // DIE DEADLINE-PRUEFUNG: Hat der Partner seit dem letzten Takt geantwortet?
    if (m_partner_Alive == false) {
        // DEADLINE VERPASST!
        std::cerr << "[ConnMgr] KRITISCHER FEHLER: Heartbeat-Deadline verpasst! Loese Not-Halt aus." << std::endl;

        m_isConnected = false; // System als getrennt markieren

        // Not-Halt Puls an den lokalen Dispatcher abfeuern
        if (m_localCoid != -1) {
            MsgSendPulse(m_localCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::DISCONNECT, 0);
        }
    } else {
        // Er hat rechtzeitig geantwortet.
        // Wir setzen die Falle wieder scharf fuer das naechste 50ms-Fenster!
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
