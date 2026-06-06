/**
 * @file errorHandler.hpp
 * @brief Zentraler Verwalter für Fehler- und Warnungszustände der Festo-Anlage.
 */

#ifndef ERROR_HANDLER_HPP
#define ERROR_HANDLER_HPP

#include "errorTypes.hpp"

namespace logik {

    class ErrorHandler {
    private:
        logik::ErrorID m_currentError;       ///< Aktiver kritischer Fehler (führt zu Stopp)
        logik::WarningID m_currentWarning;   ///< Aktive Warnung (Band läuft weiter, Gelb an)

        bool m_localChuteFull;               ///< Interner Zustand der eigenen Rutsche
        bool m_partnerChuteFull;             ///< Interner Zustand der Partner-Rutsche

        /**
         * @brief Interne Methode zur dynamischen Berechnung des Systemstatus.
         * Hier wird entschieden, ob eine Warnung oder ein kritischer Fehler vorliegt.
         */
        void evaluateChuteStates();

    public:
        /**
         * @brief Konstruktor. Initialisiert das System im fehlerfreien Zustand.
         */
        ErrorHandler();

        /**
         * @brief Standard-Destruktor.
         */
        ~ErrorHandler() = default;

        /**
         * @brief Aktualisiert den Zustand der lokalen Rutsche.
         * @param isFull true, wenn die eigene Rutsche voll ist (Lichtschranke blockiert).
         */
        void setLocalChuteStatus(bool isFull);

        /**
         * @brief Aktualisiert den Zustand der Partner-Rutsche (via Netzwerk empfangen).
         * @param isFull true, wenn die Rutsche der anderen Anlage voll ist.
         */
        void setPartnerChuteStatus(bool isFull);

        /**
         * @brief Setzt erzwungen einen kritischen Fehler (z.B. bei WP_LOST oder CONNECTION_LOST).
         * @param error Die ID des aufgetretenen kritischen Fehlers.
         */
        void triggerManualError(logik::ErrorID error );// wo grnau kommt hier noch

        /**
         * @brief Setzt das Fehlersystem zurück. Wird beim Drücken des RESET-Tasters aufgerufen.
         * Falls die Rutschen immer noch voll sind, wird der Fehler direkt neu berechnet.
         */
        void clearAll();

        /**
         * @brief Gibt den aktuell aktiven kritischen Fehler zurück.
         * @return logik::ErrorID
         */
        logik::ErrorID getError() const;

        /**
         * @brief Gibt die aktuell aktive Warnung zurück.
         * @return logik::WarningID
         */
        logik::WarningID getWarning() const;

        /**
         * @brief Prüft, ob aktuell ein kritischer Fehler das System blockiert.
         * @return true, wenn das Band sofort stoppen muss.
         */
        bool hasCriticalError() const;

        /**
         * @brief Prüft, ob aktuell eine Warnung vorliegt (für die gelbe Ampel).
         * @return true, wenn eine Warnung aktiv ist.
         */
        bool hasWarning() const;

        /**
         * @brief Gibt zurück, ob das System mechanisch bereit für einen Reset ist.
         * @return true, wenn beide Rutschen leer sind und kein externer Fehler anliegt.
         */
        bool isSystemClear() const;
    };

} // namespace logik

#endif // ERROR_HANDLER_HPP
