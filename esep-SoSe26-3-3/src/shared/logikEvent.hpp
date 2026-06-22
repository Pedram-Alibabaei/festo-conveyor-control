/**
 * @file LogikEvents.hpp
 * @brief Zentrales Event-Enum für die Schicht-1-Zustandsmaschine und Komponenten-Koppelung.
 *
 * SoSe2026 Team 3.3
 */

#ifndef LOGIK_EVENTS_HPP
#define LOGIK_EVENTS_HPP

#include <stdint.h>

namespace logik {

    /**
     * @enum Event
     * @brief Alle hardware- und softwareseitigen Steuerungsereignisse der Anlage.
     */
    enum class Event {
        // ====================================================================
        // SYSTEM-STATUS
        // ====================================================================
        UNKNOWN,                ///< Unbekanntes oder undefiniertes Ereignis

        // ====================================================================
        // ANLAGEN-SENSORIK (HARDWARE-FLANKEN VOM BAND)
        // ====================================================================
        ENTRY_BLOCKED,          ///< Lichtschranke Einlauf unterbrochen (Teil kommt an)
        ENTRY_CLEARED,          ///< Lichtschranke Einlauf frei (Teil hat Einlauf verlassen)

        HEIGHT_BLOCKED,         ///< Lichtschranke Hoehenmessung unterbrochen
        HEIGHT_CLEARED,         ///< Lichtschranke Hoehenmessung frei (Messung beendet)
        HEIGHT_ERROR,           ///< Physischer Hoehenfehler (Teil zu hoch / falsch herum)

        EJECTOR_BLOCKED,        ///< Lichtschranke vor Weiche/Auswerfer unterbrochen
        EJECTOR_CLEARED,        ///< Lichtschranke vor Weiche/Auswerfer frei

        METAL_DETECTED,         ///< Induktivsensor schlaegt an (Teil besitzt Metall)
        METAL_CLEARED,          ///< Induktivsensor wird wieder frei

        END_BLOCKED,            ///< Lichtschranke Bandende unterbrochen (Teil am Uebergabepunkt)
        END_CLEARED,            ///< Lichtschranke Bandende frei (Teil entnommen / uebergeben)

        CHUTE_BLOCKED,          ///< Lichtschranke Rutsche unterbrochen (Rutsche füllt sich)
        CHUTE_CLEARED,          ///< Lichtschranke Rutsche frei (Rutsche wurde geleert)

        // ====================================================================
        // BEDIENPANEL (TASTER)
        // ====================================================================
        START_PRESSED,          ///< Start-Taster gedrueckt
        START_RELEASED,         ///< Start-Taster losgelassen
        STOP_PRESSED,           ///< Stop-Taster gedrueckt
        STOP_RELEASED,          ///< Stop-Taster losgelassen
        RESET_PRESSED,          ///< Reset-Taster gedrueckt
        RESET_RELEASED,         ///< Reset-Taster losgelassen
        ESTOP_PRESSED,          ///< Not-Aus-Schalter gedrueckt
        ESTOP_RELEASED,         ///< Not-Aus-Schalter entriegelt

        // ====================================================================
        // NETZWERK-EREIGNISSE (QNX IPC ZWISCHEN ANLAGE 1 & 2)
        // ====================================================================
        CONNECTION_RESTORED,    ///< Netzwerkverbindung zum Partner erfolgreich hergestellt
        CONNECTION_LOST,        ///< Netzwerkverbindung zum Partner abgebrochen

        PARTNER_START_PRESSED,  ///< Start-Taster der Partneranlage wurde gedrueckt
        PARTNER_START_RELEASED, ///< Start-Taster der Partneranlage wurde losgelassen
        PARTNER_STOP_PRESSED,   ///< Stop-Taster der Partneranlage wurde gedrueckt
        PARTNER_STOP_RELEASED,  ///< Stop-Taster der Partneranlage wurde losgelassen
        PARTNER_RESET_PRESSED,  ///< Reset-Taster der Partneranlage wurde gedrueckt
        PARTNER_RESET_RELEASED, ///< Reset-Taster der Partneranlage wurde losgelassen
        PARTNER_ESTOP_PRESSED,  ///< Not-Aus der Partneranlage aktiv
        PARTNER_ESTOP_RELEASED, ///< Not-Aus der Partneranlage entriegelt
        PARTNER_ESTOP_SAFETY_SYNC, ///< Partner im Estop-Sicherheitsmodus (Taster frei, Reset noetig)
        PARTNER_CALIB_READY,       ///< Partner hat gueltige Kalibrierung
        PARTNER_CALIB_NOT_READY,   ///< Partner ist nicht kalibriert
        PARTNER_START_RUNNING,     ///< Partner startet gemeinsam Running (Ready_For_Run)
        PARTNER_START_SERVICE,     ///< Partner startet gemeinsam Service (Ready_For_Run)

        REQ_TO_GIVE_WP,         ///< Anlage 1 fragt an: "Darf ich Werkstueck uebergeben?"
        READY_TO_RECEIVE_WP,    ///< Anlage 2 antwortet: "Bereit! Ich nehme das Teil auf."
        PARTNER_HANDOVER_COMPLETE, ///< Anlage 2: WP physisch am Einlauf (Uebergabe erfolgreich)
        PARTNER_HANDOVER_WP_LOST,  ///< Anlage 2: WP bei Uebergabe verloren
        PARTNER_HANDOVER_PAUSE,    ///< Anlage 2 blockiert waehrend Uebergabe -> A1 Motor pausieren

        // ====================================================================
        // KALIBRIERUNG (DATEISYSTEM / BOOT)
        // ====================================================================
        DATA_EXIST,             ///< Kalibrierungsdatei erfolgreich geladen
        DATA_NOT_EXIST,         ///< Keine Kalibrierungsdatei vorhanden (Einlernen noetig)
        SERVICE_EMPTY_BELT_READY, ///< Service: Leerband-ADC-Referenz ist gemessen

        // ====================================================================
        // INTERNE LOGIK- UND FSM-SIGNALE
        // ====================================================================
        ENTRY_FREI,             ///< Bereich fuer neues Werkstueck ist frei gegeben
        ENTRY_NOT_FREI,         ///< Bereich ist blockiert (Einlauf-Sperre aktiv)

        ERROR,                  ///< Ein harter Anlagenfehler wurde eskaliert
        ERROR_CLEARED,          ///< Fehler wurde quittiert, Anlage wieder betriebsbereit

        START_SHORT_PRESSED,    ///< Start-Taster wurde kurz gedrueckt (< 2 Sekunden)
        START_LONG_PRESSED,     ///< Start-Taster wurde lang gedrueckt (>= 2 Sekunden -> z.B. Service-Mode)

        WARNING,                ///< Eine unkritische Warnung ist aktiv (z.B. eine Rutsche voll)
        WARNING_CLEARED,        ///< Warnung aufgehoben (Rutsche wieder leer)

        BAND_EMPTY,             ///< Keine Werkstuecke mehr im Schieberegister vorhanden
        SET_SLOW,               ///< Geschwindigkeit umschalten auf Langsamfahrt (z.B. vor Hoehenmessung)
        SET_FAST,
        MOTOR_RUN,              ///< Befehl an Motor-Aktor: Band starten
        MOTOR_STOP              ///< Befehl an Motor-Aktor: Band stoppen
    };

} // namespace logik

#endif // LOGIK_EVENTS_HPP