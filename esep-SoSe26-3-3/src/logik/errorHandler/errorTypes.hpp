/**
* @file errorTypes.hpp
 * @brief Definition der systemweiten Fehler- und Warnungs-IDs für die Festo-Anlage.
 */

#ifndef ERROR_TYPES_HPP
#define ERROR_TYPES_HPP

namespace logik {

    /**
     * @enum ErrorID
     * @brief Kritische Fehler, die zu einem harten Bandstopp (Motor STOPP)
     * und zum Blinken der roten Ampel führen. Die FSM wechselt in den StateError.
     */
    enum class ErrorID {
        NONE,               ///< Kein Fehler, System läuft normal
        BOTH_CHUTES_FULL,   ///< Kritisch: Sowohl die lokale als auch die Partner-Rutsche sind voll
        WP_LOST,            ///< Sequenzfehler: Ein Werkstück ist auf dem Band verschwunden (Timeout via Ticks)
        UNEXPECTED_EVENT,      ///< Sequenzfehler: Ein unregistriertes "Geister-Werkstück" ist am Sensor aufgetaucht
        HEIGHT_ERROR
    };

    /**
     * @enum WarningID
     * @brief Unkritische Warnungen. Das Band läuft WEITER, aber die Sortierstrategie
     * ändert sich und die gelbe Ampelleuchte wird eingeschaltet.
     */
    enum class WarningID {
        NONE,               ///< Keine Warnung aktiv
        LOCAL_CHUTE_FULL,   ///< Nur die eigene Rutsche ist voll (Anlage weicht auf Partner aus)
        PARTNER_CHUTE_FULL  ///< Nur die Partner-Rutsche ist voll (Anlage sortiert nur noch lokal aus)
    };

} // namespace logik

#endif // ERROR_TYPES_HPP