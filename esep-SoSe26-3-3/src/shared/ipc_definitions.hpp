/**
 * @file ipc_definitions.hpp
 * @brief Zentrale Definitionen für QNX Pulses und Netzwerk-Nachrichten.
 * Wird von HAL und beiden Dispatchern genutzt, um eine gemeinsame Sprache zu sprechen.
 * =======================================================================================
 * QNX PULSE MESSAGE STRUKTUR (40 Bit) - DOKUMENTATION
 * =======================================================================================
 *
 * In diesem Projekt nutzen wir QNX Pulses für die asynchrone Echtzeit-Kommunikation.
 * Ein Pulse besteht aus einem 8-Bit Code und einem 32-Bit Wert.
 *
 * Um komplexe Daten (wie "Welcher Sensor?" + "Welcher Zustand?") in einem einzigen
 * Pulse zu senden, nutzen wir "Bit-Packing" in der 32-Bit Payload.
 *
 * STRUKTUR EINES PULSES:
 * +--------------+   +---------------------------------------------------------------+
 * |  8 Bit CODE  |   |                     32 Bit VALUE (Payload)                    |
 * +--------------+   +-------------------------------+-------------------------------+
 * |              |   |  High Word (Bit 31-16)        |   Low Word (Bit 15-0)         |
 * |  SIGNAL-TYP  |   |  ---------------------        |   -------------------         |
 * |              |   |      KOMPONENTEN-ID           |      DATEN / ZUSTAND          |
 * +--------------+   +-------------------------------+-------------------------------+
 *
 * VERWENDUNG:
 * Nutzen Sie die Funktionen encodeHalPayload(), decodeSensorID() und decodeSensorState() (siehe unten),
 * um die 32-Bit Payload zu erstellen oder zu lesen.
 * =======================================================================================
 */

#ifndef IPC_DEFINITIONS_HPP_
#define IPC_DEFINITIONS_HPP_

#include <stdint.h>
#include <sys/iomsg.h>

namespace ipc {

    // ============================================================================
    // 1. PULSE CODES (Das 8-Bit Feld "code" im QNX Pulse)
    // ============================================================================
    // Unterscheidet, woher der Pulse kommt. QNX verlangt für eigene Pulses
    // Werte oberhalb von _PULSE_CODE_MINAVAIL.
    enum PulseCode : int8_t {
        // ====================================================================
        // LOKALE HARDWARE & SYSTEM (1 - 9)
        // ====================================================================
        HAL_INPUT         = _PULSE_CODE_MINAVAIL + 1,  // Kommt von der eigenen Sensorik (HAL)
        CONNECTION_MSG    = _PULSE_CODE_MINAVAIL + 2,  // Interner Handshake (ConnectionManager gefunden)
        DISCONNECT        = _PULSE_CODE_MINAVAIL + 3,  // Not-Halt oder Verbindungsabbruch

        // ====================================================================
        // TIMER EVENTS (10 - 19)
        // ====================================================================
        BELT_TICK         = _PULSE_CODE_MINAVAIL + 10, // Band-Uhr für Werkstückverfolgung
        HEARTBEATTACKT    = _PULSE_CODE_MINAVAIL + 11, // Watchdog-Timer für Netzwerk
        SORTER_TIMER_TICK = _PULSE_CODE_MINAVAIL + 12, // One-Shot Timer für die Weiche/Auswerfer

        // ====================================================================
        // NETZWERK BASIS (20 - 29)
        // ====================================================================
        NETWORK_MSG       = _PULSE_CODE_MINAVAIL + 20, // Generische Nachricht vom Partner
        ANLAGE_2_CIID     = _PULSE_CODE_MINAVAIL + 21, // Initialer Handshake über Qnet
        HEARTBEAT         = _PULSE_CODE_MINAVAIL + 22,
        // ====================================================================
        // NETZWERK WERKSTÜCK-ÜBERGABE (30 - 39)
        // ====================================================================
        WP_INFO       = _PULSE_CODE_MINAVAIL + 30, // Ersetzt dein WP_INFO (ID & Typ)
        REQ_RECEIVE   = _PULSE_CODE_MINAVAIL + 31, // Darf ich übergeben?
        READY_RECEIVE = _PULSE_CODE_MINAVAIL + 32, // Ja, ich bin bereit!
        UPDATE_LIST   = _PULSE_CODE_MINAVAIL + 33  // Liste beim Partner updaten
    };

    // ============================================================================
    // 2. HARDWARE SENSOR IDs (Für HAL_INPUT Pulses)
    // ============================================================================
    enum SensorID : uint16_t {
        // Tasten
        BTN_START     = _PULSE_CODE_MINAVAIL + 1,
        BTN_STOP,
        BTN_RESET,
        BTN_ESTOP,

        LS_ENTRY,
        LS_HEIGHT,// nicht nutzen
        LS_SWITCH,
        LS_END,
        LS_CHUTE,
        SENSOR_METAL,

        EJECTOR,
        HEIGHT

    };

    // --- Bit-Masking Hilfsfunktionen für den HAL Payload (ID + State) ---
    // Packt die 16-Bit SensorID und den 16-Bit Zustand in EINEN 32-Bit QNX Payload.

    /** @brief Wird vom HAL genutzt, um den Pulse zu bauen */
    inline uint32_t encodeHalPayload(uint16_t sensorId, uint16_t state) {
        return (static_cast<uint32_t>(sensorId) << 16) | state;
    }

    /** @brief Wird vom Dispatcher genutzt, um die Sensor-ID herauszulesen */
    inline uint16_t decodeSensorID(uint32_t payload) {
        return static_cast<uint16_t>(payload >> 16);
    }

    /** @brief Wird vom Dispatcher genutzt, um den Zustand (z.B. 1=High, 0=Low) zu lesen */
    inline uint16_t decodeSensorState(uint32_t payload) {
        return static_cast<uint16_t>(payload & 0xFFFF);
    }

    // ============================================================================
    // 3. NETZWERK NACHRICHTEN (Für NETWORK_MSG Pulses)
    // ============================================================================
    // Die globalen Events, die zwischen den beiden Dispatchern verschickt werden.
    enum NetworkMessage : uint32_t {
        GLOBAL_ESTOP_PRESSED = _PULSE_CODE_MINAVAIL + 1,
        GLOBAL_ESTOP_RELEASED,
        GLOBAL_START_PRESSED,
        GLOBAL_START_RELEASED,
        GLOBAL_RESET_PRESSED,
        GLOBAL_RESET_RELEASED,
        GLOBAL_STOP_PRESSED,
        GLOBAL_STOP_RELEASED,

        // Handover-Protokoll (Übergabe des Werkstücks)
        MSG_REQ_RECEIVE,      // Anlage 1: "Ich habe ein Werkstück am Ende"
        MSG_READY_RECEIVE,   // Anlage 2: "Alles klar, mein Einlauf ist frei"
        HANDOVER_SUCCESSFUL,          // Anlage 2: "Habe es erhalten, du kannst stoppen"
        MSG_UPDATE_LIST
    };

    enum HeightADC:uint16_t{
        BLOCKED = _PULSE_CODE_MINAVAIL + 1,
        CLEARED,
        HIGH_ERROR

    };



} // namespace ipc

#endif // IPC_DEFINITIONS_HPP_
