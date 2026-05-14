/**
* @file LogikEvents.hpp
 */
#ifndef LOGIK_EVENTS_HPP
#define LOGIK_EVENTS_HPP

#include <stdint.h>

namespace logik {

    enum class Event {

        TICK,

        ENTRY_BLOCKED,
        ENTRY_CLEARED,

        HEIGHT_BLOCKED,
        HEIGHT_CLEARED,

        EJECTOR_BLOCKED,
        EJECTOR_CLEARED,

        METAL_DETECTED,
        METAL_CLEARED,

        END_BLOCKED,
        END_CLEARED,

        CHUTE_BLOCKED,
        CHUTE_CLEARED,


        HEIGHT_VALUE_UPDATED, // is-height: Analoger Höhenwert wurde gemessen (Payload benötigt)

        // Buttons
        START_PRESSED,
        START_RELEASED,
        STOP_PRESSED,
        STOP_RELEASED,
        RESET_PRESSED,
        RESET_RELEASED,
        ESTOP_PRESSED,
        ESTOP_RELEASED,

        // Network Events
        NET_START_PRESSED,
        NET_START_RELEASED,

        NET_STOP_PRESSED,
        NET_STOP_RELEASED,

        NET_RESET_PRESSED,
        NET_RESET_RELEASED,

        NET_ESTOP_PRESSED,
        NET_ESTOP_RELEASED,

        NET_HANDOVER_READY
    };

} // namespace logik

#endif // LOGIK_EVENTS_HPP