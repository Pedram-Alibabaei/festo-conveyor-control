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
#include "logik/i_logik.h"
#include "i_Dispatcher.hpp"


class Dispatcher : public I_Dispatcher {
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
    logik::i_Logik* m_logik;      // Zeiger auf die FSM-Logik

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

    void init(ipc::AnlagenPosition pos) override;

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
    void setActuators(i_Actuators* actuators) override {
        m_actuators = actuators;
    }

    void setSensor(i_Sensor* sensor) override {
        // Not used currently in Dispatcher members, but required by interface
    }

    void setConnectionManager(i_Connection* connection) override {
        m_connection = connection;
    }

    void setTimer(i_Timer* timer) override {
        // Not used currently in Dispatcher members, but required by interface
    }

    void setLogik(logik::i_Logik* logik) override {
        m_logik = logik;
    }

    // ========================================================================
    // Ausfuehrung
    // ========================================================================
    // Die Endlosschleife!
    void run() override;

    // Stoppt die Anlage sauber
    void stop() override { m_running = false; }

    // ========================================================================
    // I_Dispatcher Hardware/Network Implementations
    // ========================================================================
    void startBeltClock(int ms) override;

    void workpieceInfo(int id, WpType type) override;
    void RequestToReceiveWp() override;
    void readyToReceiveWp() override;
    void updateWpListe(WpType type) override;

    void driveRight() override;
    void driveLeft() override;
    void driveSlowRight() override;
    void driveSlowLeft() override;
    void driveStop() override;

    void openJunction() override;
    void closeJunction() override;

    void lightGreenOn() override;
    void lightGreenOff() override;
    void lightGreenBlinkFast() override;
    void lightGreenBlinkSlow() override;

    void lightYellowOn() override;
    void lightYellowOff() override;
    void lightYellowBlinkFast() override;
    void lightYellowBlinkSlow() override;

    void lightRedOn() override;
    void lightRedOff() override;
    void lightRedBlinkFast() override;
    void lightRedBlinkSlow() override;

    void turnLEDStartOn() override;
    void turnLEDStartOff() override;
    void turnLEDStartBlinkFast() override;
    void turnLEDStartBlinkSlow() override;

    void turnLEDResetOn() override;
    void turnLEDResetOff() override;
    void turnLEDResetBlinkFast() override;
    void turnLEDResetBlinkSlow() override;

    void turnLEDQ1On() override;
    void turnLEDQ1Off() override;
    void turnLEDQ1BlinkFast() override;
    void turnLEDQ1BlinkSlow() override;

    void turnLEDQ2On() override;
    void turnLEDQ2Off() override;
    void turnLEDQ2BlinkFast() override;
    void turnLEDQ2BlinkSlow() override;

    void turnAllOff() override;
};

#endif // DISPATCHER_HPP
