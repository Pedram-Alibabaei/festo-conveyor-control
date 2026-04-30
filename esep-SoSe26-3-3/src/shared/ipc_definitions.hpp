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
#include <sys/iomsg.h> // Für _PULSE_CODE_MINAVAIL unter QNX

namespace ipc {

// ============================================================================
// 1. PULSE CODES (Das 8-Bit Feld "code" im QNX Pulse)
// ============================================================================
// Unterscheidet, woher der Pulse kommt. QNX verlangt für eigene Pulses
// Werte oberhalb von _PULSE_CODE_MINAVAIL.
enum PulseCode : int8_t {
    HAL_INPUT   = _PULSE_CODE_MINAVAIL + 1,  // Kommt von der eigenen Sensorik (HAL)
    NETWORK_MSG = _PULSE_CODE_MINAVAIL + 2   // Kommt vom anderen Steuerungscomputer (Qnet)
};


// ============================================================================
// 2. HARDWARE SENSOR IDs (Für HAL_INPUT Pulses)
// ============================================================================
// Identifiziert, WELCHER Sensor gerade ausgelöst hat.
enum SensorID : uint16_t {
    // Tasten
    II_START = 1,
    II_STOP,
    II_RESET,
    II_ESTOP,

    // Lichtschranken / Sensoren
    IP_ENTRY,
    IP_HEIGHT,
    IP_EJECTOR,
    IP_END,
    IP_CHUTE,
    IP_METAL_SENSOR,
    IP_EJECTOR_STATUS, // Zylinder eingefahren/ausgefahren

    // Spezial: Analoger Höhenwert
    HEIGHT_ADC
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
    GLOBAL_ESTOP_PRESSED = 100,
    GLOBAL_ESTOP_RELEASED,

    // Handover-Protokoll (Übergabe des Werkstücks)
    HANDOVER_READY_TO_GIVE,      // Anlage 1: "Ich habe ein Werkstück am Ende"
    HANDOVER_READY_TO_RECEIVE,   // Anlage 2: "Alles klar, mein Einlauf ist frei"
    HANDOVER_SUCCESSFUL,         // Anlage 2: "Habe es erhalten, du kannst stoppen"

    GLOBAL_SYSTEM_RESET
};


// ============================================================================
// 4. QNET NAMEN (Für name_attach und name_open)
// ============================================================================
// Diese Namen müssen im Netzwerk eindeutig sein, damit sich die Dispatcher finden.
constexpr char DISPATCHER_NAME_SYSTEM_1[] = "Steuerungscomputer_1";
constexpr char DISPATCHER_NAME_SYSTEM_2[] = "Steuerungscomputer_2";

} // namespace ipc

#endif // IPC_DEFINITIONS_HPP_