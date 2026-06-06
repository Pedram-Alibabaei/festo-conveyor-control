/**
* @file fsm_definitions.hpp
 * @brief Zentrale Typdefinitionen und Enums für die GoF-Zustandsmaschine (FSM).
 */

#ifndef FSM_DEFINITIONS_HPP
#define FSM_DEFINITIONS_HPP

namespace logik {

    /**
     * @enum StateID
     * @brief Eindeutige IDs für die übergeordneten Hauptzustände der Anlage (GoF State Pattern).
     */
    enum class StateID {
        SETUP,          ///< Initialisierungs- und Hochlaufphase der Hardware
        CALIBRATION,    ///< Dein 3-Stufen ADC-Einlernmodus (EMPTY_BELT, FLAT_WP, MIN_SAMPLES)
        STANDBY,        ///< Anlage ist betriebsbereit und wartet auf das Auflegen eines Werkstücks
        AUTOMATIC,      ///< Normaler Produktions- und Sortiermodus (Live-12-Segment-Scan läuft)
        ERROR,          ///< Störungsmodus (Eine oder beide Rutschen voll, Sequenzfehler oder Verbindungsverlust)
        ESTOP           ///< Not-Halt-Modus (Sofortiger, hardwarenaher Sicherheitsstopp)
    };

    /**
     * @enum ErrorSubState
     * @brief Unter-Zustände der Fehlersignalisierung nach Spezifikation (Absatz 90-102).
     * Regelt das Zusammenspiel aus anstehenden Hardwaresignalen und der RESET-Quittierung.
     */
    enum class ErrorSubState {
        ACTIVE_UNACKNOWLEDGED,  ///< [Spezif. 92] Fehler ist neu aufgetreten, physisch vorhanden, noch NICHT quittiert.
        ACTIVE_ACKNOWLEDGED,    ///< [Spezif. 94] Fehler steht immer noch an, wurde aber vom Bediener bereits quittiert.
        GONE_UNACKNOWLEDGED     ///< [Spezif. 98] Fehler hat sich physisch von selbst repariert/erledigt, wurde aber noch NICHT quittiert.
    };

} // namespace logik

#endif // FSM_DEFINITIONS_HPP