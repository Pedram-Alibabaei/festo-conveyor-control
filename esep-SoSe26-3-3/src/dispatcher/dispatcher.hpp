/**
 * @file Dispatcher.hpp
 * @brief Der zentrale Nachrichtenverteiler der Anlage (Reactor Pattern).
 *
 * Der Dispatcher ist der EINZIGE Thread im System, der blockiert und auf
 * asynchrone Ereignisse wartet. Er besitzt den zentralen QNX-Channel.
 */

#ifndef DISPATCHER_HPP
#define DISPATCHER_HPP

#include <sys/neutrino.h>
#include <iostream>
#include <cstdint>
#include <string>

// Eigene Includes
#include "shared/ipc_definitions.hpp"
#include "hal/i_Actuators.hpp"
#include "connectionManager/I_Connection.hpp"


class Dispatcher {
private:
    // ========================================================================
    // 1. QNX Ressourcen & Netzwerk
    // =======================================================================
    int m_chid;             // Channel ID: Der "Briefkasten" dieses Dispatchers
    int m_localCoid;        // Connection ID zu SICH SELBST (fuer den ConnectionManager)
    int m_partnerCoid;      // Connection ID zum Partner (-1 wenn nicht verbunden)

    ipc::AnlagenPosition m_position;
    std::atomic<bool> m_running; // Flag zum sauberen Beenden der run()-Schleife

    i_Actuators* m_actuators;     // Zeiger auf die Aktorik
    I_Connection* m_connection;   // Netzwerk-Management

    // ========================================================================
    // 2. Private Event-Handler & Hilfsmethoden
    // ========================================================================
    void handlePulse(const struct _pulse& pulse);
    void handleHalPulse(uint32_t payload);
    void handleNetworkPulse(uint32_t payload);
    void handleTimerPulse(int timer_id);
    void handleConnectionPulse(uint32_t payload);

public:
    // Konstruktor & Destruktor
    Dispatcher();
    ~Dispatcher();

    void init(ipc::AnlagenPosition pos);

    // ========================================================================
    // Getter
    // ========================================================================
    int getMyChannelId() const { return m_chid; }
    int getMyLocalCoid() const { return m_localCoid; }
    int getPartnerCoid() const { return m_partnerCoid; } // Umbenannt, da es eine COID ist

    std::string getAnlagenPositionName() const {
        return (m_position == ipc::AnlagenPosition::VORN_ANLAGE_1) ? "Anlage_1" : "Anlage_2";
    }

    // ========================================================================
    // Setter (Dependency Injection)
    // ========================================================================
    void setActuators(i_Actuators* actuators) {
        m_actuators = actuators;
    }

    void setConnection(I_Connection* connection) {
        m_connection = connection;
    }

    // ========================================================================
    // Ausfuehrung
    // ========================================================================
    // Die Endlosschleife!
    void run();

    // Stoppt die Anlage sauber
    void stop() { m_running = false; }
};

#endif // DISPATCHER_HPP
