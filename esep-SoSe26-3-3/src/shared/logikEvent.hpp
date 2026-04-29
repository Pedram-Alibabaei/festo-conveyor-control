#ifndef LOGIK_EVENTS_HPP
#define LOGIK_EVENTS_HPP

#include <stdint.h>

namespace logik {

    // Alle Ereignisse, auf die der Zustandsautomat reagieren muss
    // All events the state machine must react to
    enum class Event {
        // --- Buttons (ii-...) ---
        II_START_PRESSED,
        II_START_RELEASED,   // Oft wichtig, falls man auf ein langes Drücken reagieren will
        II_STOP_PRESSED,
        II_STOP_RELEASED,
        II_RESET_PRESSED,
        II_RESET_RELEASED,
        II_ESTOP_PRESSED,
        II_ESTOP_RELEASED,

        // --- Light Barriers / Photoelectric Sensors (ip-...) ---
        IP_ENTRY_BLOCKED,     // ip-entry: Werkstück im Einlauf
        IP_ENTRY_CLEARED,

        IP_HEIGHT_BLOCKED,    // ip-height: Lichtschranke am Höhensensor
        IP_HEIGHT_CLEARED,

        IP_EJECTOR_BLOCKED,   // ip-ejector: Lichtschranke an der Weiche/Auswerfer
        IP_EJECTOR_CLEARED,

        IP_END_BLOCKED,       // ip-end: Lichtschranke am Auslauf (Bandende)
        IP_END_CLEARED,

        IP_CHUTE_BLOCKED,     // ip-chute: Lichtschranke in der Rutsche (Voll/Werkstück rutscht)
        IP_CHUTE_CLEARED,

        IP_METAL_DETECTED,       // ip-metal: Metallsensor schlägt an
        IP_METAL_CLEARED,

        // --- Status / Specific Sensors (is-...) ---
        IS_EJECTOR_EXTENDED,     // is-ejector: Zylinder ist ausgefahren
        IS_EJECTOR_RETRACTED,    // is-ejector: Zylinder ist eingefahren

        IS_HEIGHT_VALUE_UPDATED, // is-height: Analoger Höhenwert wurde gemessen (Payload benötigt)

        // --- Network Events  ---
        NET_ESTOP_PRESSED,
        NET_ESTOP_RELEASED,
        NET_II_RESET_PRESSED,
        NET_II_RESET_RELEASED,
        NET_HANDOVER_READY
    };

} // namespace logik

#endif // LOGIK_EVENTS_HPP