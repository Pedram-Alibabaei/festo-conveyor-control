/**
* @file ConnectionManager.hpp
 * @brief Schlanke Implementierung des Verbindungsmanagements.
 * * Der ConnectionManager findet den Partner und überwacht die Verbindung.
 * Er sendet selbst keine Datenpakete an den Partner.
 */

#ifndef CONNECTIONMANAGER_HPP
#define CONNECTIONMANAGER_HPP

#include "I_Connection.hpp"
#include <sys/dispatch.h>
#include <atomic>
#include <thread>
#include <shared/ipc_definitions.hpp>
#include "shared/types.hpp"

class ConnectionManager : public i_Connection {
private:
    // Kommunikation zum LOKALEN Dispatcher (für Not-Halt/Fehler)
    int m_localCoid;

    // Die Adresse des PARTNERS (wird vom Dispatcher abgefragt)
    std::atomic<int> m_partnerCoid;

    AnlagenPosition m_pos;
    name_attach_t* m_attachHandle;

    // Die zentrale Status-Variable
    std::atomic<bool> m_partner_Alive;
    std::atomic<bool> m_isConnected;

    // Hintergrund-Thread für name_open (blockiert nicht)
    std::thread m_connectThread;
    std::atomic<bool> m_isSearching;
    void connectionTask();

public:
    ConnectionManager();
    virtual ~ConnectionManager();

    // Initialisierung
    void init(int localCoid, AnlagenPosition pos) override;


    void connectToPartner() override;
    void heartbeatTackt() override;
    void partnerAlive() override;

    // Status-Abfragen für den Dispatcher
    bool isConnected() const override;
    int getPartnerCoid() const override;
};

#endif
