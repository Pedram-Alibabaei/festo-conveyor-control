/**
* @file errorTypes.hpp
 * @brief Definition der spezifischen Fehler- und Warnungstypen für die Anlagenüberwachung.
 *
 * Kapselt alle anlagenspezifischen Fehler- und Warnungsursachen in stark typisierten
 * Enums, um eine präzise Diagnose und dynamische Strategiewechsel im Laborbetrieb
 * zu ermöglichen.
 *
 * SoSe2026 Team 3.3
 */

#ifndef ERROR_TYPES_HPP
#define ERROR_TYPES_HPP

namespace logik {

    /**
     * @enum ErrorType
     * @brief Stark typisiertes Enum für die zentralisierte Fehler-Eskalation (Harter Stopp).
     */
    enum class ErrorType {
        BOTH_CHUTES_FULL,   ///< Kritischer Systemstopp: Eigene UND Partner-Rutsche sind komplett voll.
        WP_LOST,            ///< Werkstück im Transit verloren: Erwartetes Teil ist physisch nicht angekommen.
        UNEXPECTED_EVENT,   ///< Sensor-Manipulation: Ein Sensor schlägt an, obwohl laut Schieberegister kein Teil dort sein darf.
        EARLY_ENTRY,        ///< Einlauf-Fehler: Werkstück am Anfang gelegt, bevor Q1 frei ist (vorderes WP hat HS nicht erreicht).
        HEIGHT_ERROR,       ///< Physischer Höhenfehler: Werkstück besitzt falsche Maße oder liegt verkehrt herum im Lichttunnel.
        STUCK               ///< Watchdog-Timeout: Ein Werkstück blockiert eine Lichtschranke für länger als 1 Sekunde (Stau).
    };

    /**
     * @enum WarningID
     * @brief Unkritische Warnungen. Das Band läuft WEITER, aber die Sortierstrategie
     * ändert sich und die gelbe Ampelleuchte wird eingeschaltet.
     */
    enum class WarningID {
        NONE,               ///< Keine Warnung aktiv (Normalbetrieb)
        LOCAL_CHUTE_FULL,   ///< Nur die eigene Rutsche ist voll (Anlage weicht auf Partner aus)
        PARTNER_CHUTE_FULL  ///< Nur die Partner-Rutsche ist voll (Anlage sortiert nur noch lokal aus)
    };

} // namespace logik

#endif // ERROR_TYPES_HPP