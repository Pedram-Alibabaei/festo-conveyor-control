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
      m_pos(UNKNOWN_POSITION),
      m_attachHandle(nullptr),
      m_partner_Alive(false),
      m_isConnected(false),
      m_isSearching(false)
{
}

ConnectionManager::~ConnectionManager() {
    // 1. Hintergrund-Thread sauber beenden
    m_isSearching = false;
    if (m_connectThread.joinable()) {
        m_connectThread.join();
    }

    // 2. QNX GNS-Registrierung aufloesen
    if (m_attachHandle != nullptr) {
        name_detach(m_attachHandle, 0);
    }

    // 3. Verbindung zum Partner schliessen
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

    //std::cout << "[ConnMgr] Initialisiere als Anlage "
         //     << (m_pos == VORN_ANLAGE_1 ? "1" : "2") << std::endl;

    // 1. Eigenen Namen zusammenbauen und bei QNX GNS anmelden
    std::string myName = (m_pos == VORN_ANLAGE_1) ? "Anlage_1" : "Anlage_2";

    // NAME_FLAG_ATTACH_GLOBAL ist extrem wichtig fuer Qnet!
    m_attachHandle = name_attach(NULL, myName.c_str(), NAME_FLAG_ATTACH_GLOBAL);

    if (m_attachHandle == nullptr) {
       // std::cerr << "[ConnMgr] FEHLER: Konnte GNS-Namen " << myName << " nicht registrieren!" << std::endl;
        return; // Abbruch bei Fehler
    }
   // std::cout << "[ConnMgr] Erfolgreich als '" << myName << "' registriert." << std::endl;

    // ========================================================================
    // 2. DEINE ASYMMETRISCHE START-LOGIK
    // ========================================================================
    if (m_pos == HINTEN_ANLAGE_2) {
      //  std::cout << "[ConnMgr] Ich bin Anlage 2. Starte sofort die aktive Suche..." << std::endl;
        connectToPartner(); // Startet den Thread
    } else {
        //std::cout << "[ConnMgr] Ich bin Anlage 1. Warte passiv im Hintergrund." << std::endl;
        // Ende der Methode. Thread wird nicht gestartet!
    }
}


void ConnectionManager::connectToPartner() {
    // Sicherheits-Check: Nur starten, wenn wir nicht schon suchen oder verbunden sind
    if (!m_isSearching && !m_isConnected) {

        // ====================================================================
        // NEU: ZOMBIE-THREAD AUFRÄUMEN!
        // Bevor wir einen neuen Thread starten, müssen wir den alten,
        // bereits beendeten Thread sauber aus dem Speicher löschen.
        // ====================================================================
        if (m_connectThread.joinable()) {
            m_connectThread.join();
        }

        m_isSearching = true;
        // Jetzt ist der Platz frei und wir können sicher einen neuen Thread starten
        m_connectThread = std::thread(&ConnectionManager::connectionTask, this);
    }
}

// ============================================================================
// Der Hintergrund-Thread (Lazy Loading)
// ============================================================================

void ConnectionManager::connectionTask() {
    // ====================================================================
    // NEU: ALTE VERBINDUNG IM HINTERGRUND TRENNEN
    // Wenn dieser Befehl jetzt 5 Sekunden blockiert, ist das völlig egal,
    // da die Anlage bereits sicher im Not-Halt steht!
    // ====================================================================
    if (m_partnerCoid != -1) {
        ConnectDetach(m_partnerCoid);
        m_partnerCoid = -1;
    }

    std::string targetName = (m_pos == VORN_ANLAGE_1) ? "Anlage_2" : "Anlage_1";
    std::string fullPath = "/dev/name/global/" + targetName;

    while (m_isSearching) {
        if (access(fullPath.c_str(), F_OK) != -1) {
            int coid = name_open(targetName.c_str(), NAME_FLAG_ATTACH_GLOBAL);

            if (coid != -1) {
                m_partnerCoid = coid;
                m_partner_Alive = true;
                m_isConnected = true;

                if (m_localCoid != -1) {
                    MsgSendPulse(m_localCoid, 1, ipc::CONNECTION_MSG, coid);
                }

                m_isSearching = false;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
// ============================================================================
// Hard-Realtime Kontaktueberwachung (Watchdog)
// ============================================================================

void ConnectionManager::heartbeatTackt() {
    if (!m_isConnected) return;

    if (m_partner_Alive == false) {
        m_isConnected = false;

        // 1. Dispatcher SOFORT informieren (löst den Not-Halt im Millisekunden-Bereich aus!)
        if (m_localCoid != -1) {
            MsgSendPulse(m_localCoid, SIGEV_PULSE_PRIO_INHERIT, ipc::DISCONNECT, 0);
        }

        // 2. Hintergrund-Thread starten. Das Aufräumen von m_partnerCoid passiert jetzt dort!
        connectToPartner();
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
