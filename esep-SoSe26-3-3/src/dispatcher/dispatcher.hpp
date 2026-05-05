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

// Eure eigenen Includes
#include "shared/ipc_definitions.hpp"
#include "hal/i_Actuators.hpp"
// Forward Declaration für die Logik (damit wir den Header hier nicht einbinden müssen)
// class SortierLogik;

class Dispatcher {
private:
    // ========================================================================
    // 1. QNX Ressourcen
    // ========================================================================
    int m_chid; // Channel ID: Der "Briefkasten" dieses Dispatchers


    i_Actuators* m_actuators; // NEU: Zeiger auf die Aktorik
    // ========================================================================
    // 2. Abhängigkeiten (Das "Gehirn", dem wir die Daten geben)
    // ========================================================================
    // std::shared_ptr<SortierLogik> m_logik;

    // ========================================================================
    // 3. Private Event-Handler (Verteiler-Methoden)
    // ========================================================================
    /**
     * @brief Wird aufgerufen, wenn ein Pulse vom HAL (Sensor/ADC) ankommt.
     * @param payload Die 32-Bit Daten (Sensor-ID + Wert), die im Pulse stecken.
     */
    void handleHalPulse(uint32_t payload);

    /**
     * @brief Wird aufgerufen, wenn ein Timer abläuft (z.B. Werkstück verloren).
     * @param timer_id Die ID des Timers, der abgelaufen ist.
     */
    void handleTimerPulse(int timer_id);

public:
    /**
     * @brief Konstruktor. Erstellt automatisch den QNX Channel.
     */
    Dispatcher(/* std::shared_ptr<SortierLogik> logik */);

    /**
     * @brief Destruktor. Zerstört den Channel sauber.
     */
    ~Dispatcher();

    /**
     * @brief Gibt die Channel-ID zurück.
     * Sehr wichtig, damit die main.cpp eine Connection (coid) für den HAL
     * erstellen kann, damit der HAL weiß, wohin er die Pulses senden soll!
     */
    int getChannelId() const;

    // NEU: Setter-Methode, um die Aktorik zu übergeben
    void setActuators(i_Actuators* actuators) {
    	m_actuators = actuators;
    }
    /**
     * @brief Die Endlosschleife! Blockiert und wartet auf MsgReceivePulse().
     */
    void run();
};

#endif // DISPATCHER_HPP
