/**
 * @file errorHandler.hpp
 * @brief Zeitliche Überwachung von Sensoren (Watchdogs) und zentrales Fehlermanagement.
 *
 * Diese Komponente überwacht kritische Zeitfenster der Hardware-Sensoren und
 * nimmt detaillierte Fehlermeldungen aus dem gesamten System entgegen, um
 * spezifische Fehlermaßnahmen einzuleiten.
 *
 * SoSe2026 Team 3.3
 */

#ifndef ERROR_HANDLER_HPP
#define ERROR_HANDLER_HPP

#include "shared/logikEvent.hpp"
#include "errorTypes.hpp" // Enthält das Enum class ErrorType
#include "shared/types.hpp"
#include <chrono>

namespace logik {

    class Logik; // Vorwärtsdeklaration der Haupt-Fassade zur Vermeidung von Kreisabhängigkeiten

    class ErrorHandler {
    private:
        Logik* m_context; ///< Zeiger auf den übergeordneten Hauptkontext (Nicht-besitzend)

        // --- Zeitmessung für Einlauf (Entry) ---
        bool m_isEntryTimerRunning;
        std::chrono::steady_clock::time_point m_entryStartTime;

        // --- Zeitmessung für Rutsche (Chute) ---
        bool m_isChuteTimerRunning;
        std::chrono::steady_clock::time_point m_chuteStartTime;
        bool m_isChuteWarningActive; ///< Verhindert doppeltes Senden des Warning-Events im Takt

        // Konstante für das allgemeine Watchdog-Timeout (1 Sekunde)
        const std::chrono::milliseconds TIMEOUT_THRESHOLD;

    public:
        /**
         * @brief Konstruktor.
         * @param context Zeiger auf die übergeordnete Logik-Schaltzentrale.
         */
        explicit ErrorHandler(Logik* context);

        /**
         * @brief Standard-Destruktor.
         */
        ~ErrorHandler() = default;

        // Verhindert unerlaubtes Kopieren der Überwachungskomponente
        ErrorHandler(const ErrorHandler&) = delete;
        ErrorHandler& operator=(const ErrorHandler&) = delete;

        /**
         * @brief Wird vom EventManager aufgerufen, wenn Hardware-Events eintreffen.
         * @param e Das aufgetretene Sensor-Event (z.B. ENTRY_BLOCKED).
         */
        void processEvent(logik::Event e);

        /**
         * @brief Der zentrale Herzschlag (zyklischer Aufruf alle 10ms).
         * @details Prüft fortlaufend, ob die gestarteten Watchdog-Timer das Zeitlimit reißen.
         */
        void takt();

        // ========================================================================
        // ZENTRALE ESKALATIONS-SCHNITTSTELLE
        // ========================================================================
        /**
         * @brief Nimmt detaillierte Fehlermeldungen aus allen Systemkomponenten entgegen.
         */
        void handleEscalation(ErrorType type,
                              logik::Event triggerEvent = logik::Event::UNKNOWN,
                              WpLocation location = WpLocation::UNKNOWN_LOCATION,
                              int wpId = -1);

        /** Chute-Watchdog laeuft oder Warnung bereits ausgeloest. */
        bool isChuteOccupancyWatchActive() const {
            return m_isChuteTimerRunning || m_isChuteWarningActive;
        }

        /** Setzt isChuteFull + WARNING, falls Watchdog noch nicht eskaliert hat. */
        void ensureChuteWarningRaised();

        /** Lichtschranke frei oder CHUTE_CLEARED -> Warning zuruecksetzen. */
        void clearChuteWarningState();

        /** Nach ErrorState: Warning nur wenn Rutsche physisch noch blockiert. */
        void reconcileChuteWarningAfterErrorRecovery();

        /**
         * Betriebsstart (Idle/Error -> Running): Rutsche per LS-Level pruefen.
         * Keine Flanke noetig – auch wenn CHUTE_BLOCKED schon vor Running aktiv war.
         */
        void reconcileChuteWarningOnOperationalEntry();

    private:
        bool isChutePhysicallyBlocked() const;
        void raiseChuteFullWarning();
        void updateChuteLevelWatchdog();
    };

} // namespace logik

#endif // ERROR_HANDLER_HPP