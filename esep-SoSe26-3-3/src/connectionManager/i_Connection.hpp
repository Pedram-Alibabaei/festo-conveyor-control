/**
* @file I_Connection.hpp
 * @brief Schnittstelle fuer die QNX-Netzwerkkommunikation und Kontaktueberwachung.
 * * Wird vom Dispatcher genutzt, um Nachrichten zu senden, Verbindungen
 * aufzubauen und den Watchdog (Heartbeat) zurueckzusetzen.
 */

#ifndef I_CONNECTION_HPP
#define I_CONNECTION_HPP

#include <cstdint>
#include <shared/ipc_definitions.hpp>
#include "shared/types.hpp"

class i_Connection {
public:
    virtual ~i_Connection() = default;

    // 1. Initialisiert den Briefkasten zum lokalen Dispatcher (pulseCode entfernt!)
    virtual void init(int coid, AnlagenPosition pos) = 0;

    // 3. Startet den Verbindungsaufbau (name_open)
    virtual void connectToPartner() = 0;

    // 4. Wird vom Dispatcher aufgerufen, wenn der zentrale Timer getickt hat
    virtual void heartbeatTackt() = 0;

    // 5. wird vom Dispatcher aufgerufen, wenn ein Puls vom Partner ankommt.
    virtual void partnerAlive() = 0;

    // 6. Status-Abfrage (Tür offen oder zu?)
    virtual bool isConnected() const = 0;

    // 7. Zieladresse für den Dispatcher
    virtual int getPartnerCoid() const = 0;
};

#endif // I_CONNECTION_HPP
