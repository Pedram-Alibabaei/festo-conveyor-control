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
        HEIGHT_ERROR,

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
        PARTNER_START_PRESSED,
        PARTNER_START_RELEASED,

        PARTNER_STOP_PRESSED,
        PARTNER_STOP_RELEASED,

        PARTNER_RESET_PRESSED,
        PARTNER_RESET_RELEASED,

        PARTNER_ESTOP_PRESSED,
        PARTNER_ESTOP_RELEASED,

        CONNECTION_LOST,
        CONNECTION_RESTORED,
        REQ_TO_GIVE_WP,
        READY_TO_RECEIVE_WP,
    };

} // namespace logik

#endif // LOGIK_EVENTS_HPP