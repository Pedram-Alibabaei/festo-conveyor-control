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
    enum PulseCode {
        // ====================================================================
        // LOKALE HARDWARE & SYSTEM (1 - 9)
        // ====================================================================
        HAL_INPUT                = _PULSE_CODE_MINAVAIL + 1,  // Kommt von der eigenen Sensorik (HAL)
        HAL_ESTOP                = _PULSE_CODE_MINAVAIL + 2,  // Not-Halt-Taster
        HAL_ADC_CALIBRATION      = _PULSE_CODE_MINAVAIL + 3,  // Rohwerte von der HAL während Messung
        // MANAGER_ADC_CALIBRATION  = _PULSE_CODE_MINAVAIL + 4,  // Geladene ADC-Werte vom CalibrationManager beim Start
        // MANAGER_TICK_CALIBRATION = _PULSE_CODE_MINAVAIL + 5,  // Geladene Ticks vom CalibrationManager beim Start
        CONNECTION_MSG           = _PULSE_CODE_MINAVAIL + 6,  // Interner Handshake (ConnectionManager gefunden)
        DISCONNECT               = _PULSE_CODE_MINAVAIL + 7,  // Not-Halt oder Verbindungsabbruch
        RECONNECT_MSG            = _PULSE_CODE_MINAVAIL + 8,  // Verzoegerte Partnersuche (nicht im Heartbeat blockieren)

        // ====================================================================
        // TIMER EVENTS (10 - 19)
        // ====================================================================
        BELT_TICK         = _PULSE_CODE_MINAVAIL + 10, // Band-Uhr für Werkstückverfolgung
        HEARTBEATTACKT    = _PULSE_CODE_MINAVAIL + 11, // Watchdog-Timer für Netzwerk
        //SORTER_TIMER_TICK = _PULSE_CODE_MINAVAIL + 12, // One-Shot Timer für die Weiche/Auswerfer

        // ====================================================================
        // NETZWERK BASIS (20 - 29)
        // ====================================================================
        PARTNER_MSG       = _PULSE_CODE_MINAVAIL + 20, // Generische Nachricht vom Partner
        ANLAGE_2_CIID     = _PULSE_CODE_MINAVAIL + 21, // Initialer Handshake über Qnet
        HEARTBEAT         = _PULSE_CODE_MINAVAIL + 22,
        PARTNER_ESTOP      = _PULSE_CODE_MINAVAIL + 23,

        // ====================================================================
        // NETZWERK WERKSTÜCK-ÜBERGABE (30 - 39)
        // ====================================================================
        WP_INFO       = _PULSE_CODE_MINAVAIL + 30, // Ersetzt dein WP_INFO (ID & Typ)
        UPDATE_LIST   = _PULSE_CODE_MINAVAIL + 33  // Liste beim Partner updaten
    };

    // ============================================================================
    // 2. HARDWARE SENSOR IDs (Für HAL_INPUT Pulses)
    // ============================================================================
    enum SensorID : uint16_t {
        // Tasten
        BTN_START        = _PULSE_CODE_MINAVAIL + 1,
        BTN_STOP         = _PULSE_CODE_MINAVAIL + 2,
        BTN_RESET        = _PULSE_CODE_MINAVAIL + 3,
        BTN_ESTOP        = _PULSE_CODE_MINAVAIL + 4,

        LS_ENTRY         = _PULSE_CODE_MINAVAIL + 5,
        LS_HEIGHT        = _PULSE_CODE_MINAVAIL + 6,// nicht nutzen
        LS_SWITCH        = _PULSE_CODE_MINAVAIL + 7,
        LS_END           = _PULSE_CODE_MINAVAIL + 8,
        LS_CHUTE         = _PULSE_CODE_MINAVAIL + 9,
        SENSOR_METAL     = _PULSE_CODE_MINAVAIL + 10,

        EJECTOR          = _PULSE_CODE_MINAVAIL + 11,
        HEIGHT           = _PULSE_CODE_MINAVAIL + 12,

    };

    // ============================================================================
    // 3. NETZWERK NACHRICHTEN (Für NETWORK_MSG Pulses)
    // ============================================================================
    // Die globalen Events, die zwischen den beiden Dispatchern verschickt werden.
    enum NetworkMessage : uint32_t {
        REQ_RECEIVE           = _PULSE_CODE_MINAVAIL + 1, // Darf ich übergeben?
        READY_RECEIVE         = _PULSE_CODE_MINAVAIL + 2, // Ja, ich bin bereit!
        HANDOVER_SUCCESSFUL   = _PULSE_CODE_MINAVAIL + 3,
        CHUTE                 = _PULSE_CODE_MINAVAIL + 4,
        ESTOP_STATUS_QUERY    = _PULSE_CODE_MINAVAIL + 5, // Partner ESTOP-Status anfragen
        CALIB_STATUS_QUERY    = _PULSE_CODE_MINAVAIL + 6, // Partner Kalibrierungsstatus anfragen
        CALIB_STATUS          = _PULSE_CODE_MINAVAIL + 7, // Kalibrierungsstatus melden
        START_RUNNING         = _PULSE_CODE_MINAVAIL + 8, // Gemeinsamer Running-Start
        START_SERVICE         = _PULSE_CODE_MINAVAIL + 9, // Gemeinsamer Service-Start
 };

    enum State:uint16_t{
        BLOCKED    = _PULSE_CODE_MINAVAIL + 1,
        CLEARED    = _PULSE_CODE_MINAVAIL + 2,
        PRESSED    = _PULSE_CODE_MINAVAIL + 3,
        RELEASED   = _PULSE_CODE_MINAVAIL + 4,
        VOLL       = _PULSE_CODE_MINAVAIL + 5,
        EMPTY      = _PULSE_CODE_MINAVAIL + 6,

        /** Partner-Taster frei, aber noch im Estop-Sicherheitsmodus (Reset noetig). */
        ESTOP_SAFETY_WAIT = _PULSE_CODE_MINAVAIL + 7,

        HIGH_ERROR = _PULSE_CODE_MINAVAIL + 10,

    };
    // Sub-Typen für Kalibrierungs-Payloads (Sowohl ADC als auch Ticks!)
    enum class CalibMode : uint16_t {
        // 1. ADC-Schwellen (Zustände & Rohwerte)
        EMPTY_BELT                  = 1,
        FLAT_WP                     = 2, // FlatCenter
        MIN_SAMPLES                 = 3,

        // Berechnete Toleranzgrenzen für die FSM-Typerkennung (Erweiterung)
        FLAT_MIN                    = 4,
        FLAT_MAX                    = 5,
        TALL_MIN                    = 6,
        TALL_MAX                    = 7,
        DRILLED_MIN                 = 8,
        DRILLED_MAX                 = 9,
        MAX_HEIGHT                  =10,

        // 2. Wegstrecken (Ticks)
        TICK_ENTRY_C_TO_HEIGHT_B_SLOW       = 11,
        TICK_HEIGHT_B_TO_HEIGHT_C_SLOW      = 12,
        TICK_ENTRY_C_TO_HEIGHT_B_NORMAL     = 13,
        TICK_HEIGHT_B_TO_HEIGHT_C_NORMAL    = 14,
        TICK_HEIGHT_C_TO_EJECTOR_B_NORMAL   = 15,
        TICK_EJECTOR_B_TO_EJECTOR_C_NORMAL  = 16,
        TICK_EJECTOR_C_TO_END_B_NORMAL      = 17,
        TICK_END_B_TO_END_C_NORMAL          = 18
    };


    /**
     * @brief Hilfsmethode zum Packen der Kalibrierungs-Payload.
     * @param value Der eigentliche Kalibrierwert (ADC-Wert oder Sample-Anzahl).
     * @param mode Der Sub-Typ aus dem Namespace CalibMode.
     * * Struktur im 32-Bit Integer:
     * [ Bit 31 ... 16: Kalibrierwert (16 Bit) ] [ Bit 15 ... 0: CalibMode-ID (16 Bit) ]
     */
    inline uint32_t encodeCalibPayload(uint16_t value, uint16_t mode) {
        return (static_cast<uint32_t>(value) << 16) | static_cast<uint32_t>(mode);
    }

    inline void decodeCalibPayload(uint32_t payload, uint16_t& value, uint16_t& mode) {
        value = static_cast<uint16_t>(payload >> 16);
        mode = static_cast<uint16_t>(payload & 0xFFFF);
    }

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


} // namespace ipc

#endif // IPC_DEFINITIONS_HPP_
