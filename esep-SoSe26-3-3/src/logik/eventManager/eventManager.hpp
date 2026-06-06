/**
 * @file eventManager.hpp
 * @brief Zeitliche Filterung, Entprellung und Vorverarbeitung von Hardware-Signalen.
 */

#ifndef EVENT_MANAGER_HPP
#define EVENT_MANAGER_HPP

#include "logik/i_Logik.hpp" // Enthält das globale logik::Event-Enum
#include <chrono>

namespace logik {

    class Logik;

    class EventManager {
    private:
        Logik* m_context; ///< Zeiger auf die Haupt-Logik (der Context) zur Event-Weiterleitung

        // ====================================================================
        // ZEITSTRECKEN-FILTER (RUTSCHEN-ÜBERWACHUNG)
        // ====================================================================
        std::chrono::steady_clock::time_point m_chuteBlockedStartTime;
        bool m_isChuteTimerRunning;
        const std::chrono::milliseconds CHUTE_FILTER_TIME; ///< Standard: 2000ms (2 Sekunden)

        // ====================================================================
        // TASTER-ZEITMESSUNG (START-TASTER: KURZ VS. LANG)
        // ====================================================================
        std::chrono::steady_clock::time_point m_startPressedStartTime;
        bool m_isStartTimerRunning;
        const std::chrono::milliseconds START_LONG_THRESHOLD; ///< Standard: 3000ms (3 Sekunden)

        // ====================================================================
        // NEU: EINLAUF-ÜBERWACHUNG (ENTRY BLOCKED TIMEOUT)
        // ====================================================================
        std::chrono::steady_clock::time_point m_entryBlockedStartTime;
        bool m_isEntryTimerRunning;
        const std::chrono::milliseconds ENTRY_FILTER_TIME; ///< Standard: 2000ms (2 Sekunden)

        // ====================================================================
        // NEU: BANDENDE-BLOCKADE FLAG
        // ====================================================================
        bool m_isEndBlocked; ///< true, wenn ein Teil am Bandende steht und den Motor blockiert

    public:
        /**
         * @brief Konstruktor.
         * @param context Zeiger auf die übergeordnete Logik-Komponente.
         */
        explicit EventManager(Logik* context);

        /**
         * @brief Standard-Destruktor.
         */
        ~EventManager() = default;

        /**
         * @brief Nimmt die rohen, ungefilterten Events direkt vom Dispatcher entgegen.
         * HIER wird nun auch das END_BLOCKED Flag geprüft, um Motorstarts zu blockieren.
         * @param rawEvent Das rohe Event von den HAL-Sensoren.
         */
        void handleRawEvent(logik::Event rawEvent);

        /**
         * @brief Zyklische Update-Methode (muss im System-Timer-Takt, z.B. alle 10ms, aufgerufen werden).
         * Überprüft Rutschen-Timeout und das neue Einlauf-Timeout.
         */
        void update();

        /**
         * @brief Setzt alle internen Filter, Zeitstempel und Flags zurück.
         */
        void resetFilters();

        /**
         * @brief Getter, um abzufragen, ob das Bandende aktuell blockiert ist.
         * Kann von der Hauptlogik oder den States direkt genutzt werden.
         * @return true, wenn das Bandende belegt ist.
         */
        bool isEndBlocked() const { return m_isEndBlocked; }
    };

} // namespace logik

#endif // EVENT_MANAGER_HPP