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

// Definition der Position im Gesamtsystem
enum class AnlagenPosition {
    VORN_ANLAGE_1,
    HINTEN_ANLAGE_2
};

class Dispatcher {
private:
    // ========================================================================
    // 1. QNX Ressourcen & Netzwerk
    // =======================================================================
    int m_chid;             // Channel ID: Der "Briefkasten" dieses Dispatchers
    int partner_coid;       // Connection ID zum Partner (-1 wenn nicht verbunden)
    std::string myName;
    std::string partnerName;
    AnlagenPosition m_position;

    i_Actuators* m_actuators; // Zeiger auf die Aktorik

    // ========================================================================
    // 2. Private Event-Handler & Hilfsmethoden
    // ========================================================================
    void handleHalPulse(uint32_t payload);
    void handleNetworkPulse(uint32_t payload);
    void handleTimerPulse(int timer_id);

    void registingNameInGNS();
    void setAnlagenPosition(bool pos);
    void searchForNeighbor();

public:
    // Konstruktor & Destruktor
    Dispatcher(bool pos);
    ~Dispatcher();

    // Getter
    int getMyChannelId() const { return m_chid; }
    int getPartnerChannelId() const { return partner_coid; }
    std::string getAnlagenPosition() { return myName; }

    // Setter
    void setActuators(i_Actuators* actuators) {
        m_actuators = actuators;
    }

    // Die Endlosschleife!
    void run();
};

#endif // DISPATCHER_HPP