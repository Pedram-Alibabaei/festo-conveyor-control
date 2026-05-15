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
#include <atomic>

// Eigene Includes
#include "shared/ipc_definitions.hpp"
#include "shared/types.hpp"
#include "hal/i_Actuators.hpp"
#include "hal/i_Sensors.hpp"
#include "connectionManager/I_Connection.hpp"
#include "timer/i_Timer.hpp"
//#include "logik/i_Logik.hpp" // Sobald die Datei existiert, einkommentieren!

// NEU: Das Interface inkludieren!
#include "I_Dispatcher.hpp"

#include <sys/iomsg.h>
#include <sys/dispatch.h>

// Vorwärtsdeklaration für Logik, falls der Include noch auskommentiert ist
class i_Logik;

// ============================================================================
// VERERBUNG: Dispatcher implementiert nun I_Dispatcher
// ============================================================================
class Dispatcher : public I_Dispatcher {
private:
    // ========================================================================
    // 1. QNX Ressourcen & Netzwerk
    // =======================================================================
    int m_chid;             // Channel ID: Der "Briefkasten" dieses Dispatchers
    int m_localCoid;        // Connection ID zu SICH SELBST
    int m_partnerCoid;      // Connection ID zum Partner (-1 wenn nicht verbunden)
    name_attach_t* m_attachHandle;

    AnlagenPosition m_position;
    std::atomic<bool> m_running;

    // Komponenten-Pointer
    i_Actuators* m_actuators;
    i_Sensors* m_sensors;
    i_Connection* m_connection;
    i_Timer* m_timer;
    i_Logik* m_logik; // Wieder aktiv!

    // ========================================================================
    // 2. Private Event-Handler & Hilfsmethoden
    // ========================================================================
    void handlePulse(const struct _pulse& pulse);
    void handleHalPulse(uint32_t payload);
    void handleNetworkPulse(uint32_t payload);
    void handleTimerPulse(int timer_id);
    void handleConnectionPulse(uint32_t payload);

public:
    Dispatcher();
    ~Dispatcher() override; // Virtual Destructor override

    void init(AnlagenPosition pos) override;
    void run() override;
    void stop() override;

    // ========================================================================
    // Eigene Getter (nicht im Interface, aber wichtig für main.cpp)
    // ========================================================================
    int getMyChannelId() const { return m_chid; }
    int getMyLocalCoid() const { return m_localCoid; }
    std::string getAnlagenPositionName() const {
        return (m_position == VORN_ANLAGE_1) ? "Anlage_1" : "Anlage_2";
    }

    // ========================================================================
    // Dependency Injection (Setter aus Interface)
    // ========================================================================
    void setActuators(i_Actuators* actuators) override { m_actuators = actuators; }
    void setSensor(i_Sensors* sensor) override { m_sensors = sensor; }
    void setConnectionManager(i_Connection* connection) override { m_connection = connection; }
    void setTimer(i_Timer* timer) override { m_timer = timer; }
    void setLogik(i_Logik* logik) override { m_logik = logik; }

    // ========================================================================
    // DURCHREICHE-METHODEN (Fassade aus Interface implementieren)
    // ========================================================================

    // Timer
    void startBeltClock(int ms) override;

    // Netzwerk
    void workpieceInfo(int id, WpType type) override;
    void RequestToReceiveWp() override;
    void readyToReceiveWp() override;
    void updateWpListe(WpType type) override;

   // ========================================================================
    // AKTORIK - Durchreiche-Methoden (Inline)
    // ========================================================================

    // Motorsteuerung
    void driveRight() override { if (m_actuators) m_actuators->driveRight(); }
    void driveLeft() override { if (m_actuators) m_actuators->driveLeft(); }
    void driveSlowRight() override { if (m_actuators) m_actuators->driveSlowRight(); }
    void driveSlowLeft() override { if (m_actuators) m_actuators->driveSlowLeft(); }
    void driveStop() override { if (m_actuators) m_actuators->driveStop(); }

    // Sortiereinheit
    void openJunction() override { if (m_actuators) m_actuators->openJunction(); }
    void closeJunction() override { if (m_actuators) m_actuators->closeJunction(); }

    // Ampelsteuerung
    void lightGreenOn() override { if (m_actuators) m_actuators->lightGreenOn(); }
    void lightGreenOff() override { if (m_actuators) m_actuators->lightGreenOff(); }
    void lightGreenBlinkFast() override { if (m_actuators) m_actuators->lightGreenBlinkFast(); }
    void lightGreenBlinkSlow() override { if (m_actuators) m_actuators->lightGreenBlinkSlow(); }

    void lightYellowOn() override { if (m_actuators) m_actuators->lightYellowOn(); }
    void lightYellowOff() override { if (m_actuators) m_actuators->lightYellowOff(); }
    void lightYellowBlinkFast() override { if (m_actuators) m_actuators->lightYellowBlinkFast(); }
    void lightYellowBlinkSlow() override { if (m_actuators) m_actuators->lightYellowBlinkSlow(); }

    void lightRedOn() override { if (m_actuators) m_actuators->lightRedOn(); }
    void lightRedOff() override { if (m_actuators) m_actuators->lightRedOff(); }
    void lightRedBlinkFast() override { if (m_actuators) m_actuators->lightRedBlinkFast(); }
    void lightRedBlinkSlow() override { if (m_actuators) m_actuators->lightRedBlinkSlow(); }

    // Bedienpanel-LEDs
    void turnLEDStartOn() override { if (m_actuators) m_actuators->turnLEDStartOn(); }
    void turnLEDStartOff() override { if (m_actuators) m_actuators->turnLEDStartOff(); }
    void turnLEDStartBlinkFast() override { if (m_actuators) m_actuators->turnLEDStartBlinkFast(); }
    void turnLEDStartBlinkSlow() override { if (m_actuators) m_actuators->turnLEDStartBlinkSlow(); }

    void turnLEDResetOn() override { if (m_actuators) m_actuators->turnLEDResetOn(); }
    void turnLEDResetOff() override { if (m_actuators) m_actuators->turnLEDResetOff(); }
    void turnLEDResetBlinkFast() override { if (m_actuators) m_actuators->turnLEDResetBlinkFast(); }
    void turnLEDResetBlinkSlow() override { if (m_actuators) m_actuators->turnLEDResetBlinkSlow(); }

    void turnLEDQ1On() override { if (m_actuators) m_actuators->turnLEDQ1On(); }
    void turnLEDQ1Off() override { if (m_actuators) m_actuators->turnLEDQ1Off(); }
    void turnLEDQ1BlinkFast() override { if (m_actuators) m_actuators->turnLEDQ1BlinkFast(); }
    void turnLEDQ1BlinkSlow() override { if (m_actuators) m_actuators->turnLEDQ1BlinkSlow(); }

    void turnLEDQ2On() override { if (m_actuators) m_actuators->turnLEDQ2On(); }
    void turnLEDQ2Off() override { if (m_actuators) m_actuators->turnLEDQ2Off(); }
    void turnLEDQ2BlinkFast() override { if (m_actuators) m_actuators->turnLEDQ2BlinkFast(); }
    void turnLEDQ2BlinkSlow() override { if (m_actuators) m_actuators->turnLEDQ2BlinkSlow(); }

    void turnAllOff() override { if (m_actuators) m_actuators->turnAllOff(); }
};

#endif // DISPATCHER_HPP
