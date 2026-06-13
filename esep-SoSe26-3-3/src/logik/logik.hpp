/**
 * @file Logik.hpp
 * @brief Konkrete Implementierung der zentralen Steuerungskomponente (Zustandsmaschine).
 *
 * @details Diese Klasse verknüpft die übergeordneten Schicht-1-Zustände der Festo-Anlage
 * mit dem zeittaktbasierten Schieberegister (WorkpieceManager). Sie implementiert das
 * Interface i_Logik für die netzwerkweite Kommunikation.
 *
 * SoSe2026 Team 3.3
 */

#ifndef LOGIK_HPP
#define LOGIK_HPP

#include "i_Logik.hpp"
#include "workpieceManager/WorkpieceManager.hpp"
#include "eventManager/eventManager.hpp"
#include "errorHandler/errorHandler.hpp" // NEU: Header für den zentralen ErrorHandler inkludieren
#include "dispatcher/i_Dispatcher.hpp"
#include <memory>

class I_CalibrationManager; // Vorwärtsdeklaration des CalibrationManager-Interfaces

namespace logik {

    class BaseState;

    class Logik : public ::i_Logik {
    private:
        // --- Infrastruktur-Komponenten ---
        std::unique_ptr<WorkpieceManager> m_wpManager; ///< Das zeittaktbasierte Schieberegister (Digitaler Zwilling)
        std::unique_ptr<ErrorHandler> m_errorHandler;   ///< NEU: Der zentrale ErrorHandler (Wächter über Timeouts & Eskalationen)
        I_Dispatcher* m_dispatcher;                    ///< Zeiger auf den Dispatcher für HW-Ausgaben und Netzwerknachrichten
        I_CalibrationManager* m_calibManager;          ///< Zeiger auf den CalibrationManager für Datei-Operationen
        EventManager* m_eventManager;                  ///< Zeiger auf den EventManager (FSM-Schnittstelle)

        // --- Kalibrierungsdaten (Zentral in der Logik verwaltet) ---
        CalibrationData m_calibData;                   ///< Geladene Zeit-Kalibrierungsdaten (aus types.hpp)

        // --- Interne Zustandsvariablen ---
        AnlagenPosition m_anlagenPosition;             ///< Die dieser Instanz zugewiesene physische Position (Anlage 1 oder 2)
        bool m_isPartnerChuteFull;                     ///< Statusflag für die Rutsche der jeweils anderen Teilanlage
        bool m_isMotorSlow;                            ///< Als Member-Variable statt global deklariert

    public:
        /**
         * @brief Konstruktor.
         * @param dispatcher Zeiger auf das Kommunikations- und Hardware-Interface.
         */
        explicit Logik(I_Dispatcher* dispatcher);

        /**
         * @brief Destruktor.
         */
        virtual ~Logik() override = default;

        // ========================================================================
        // SETTER FÜR DEPENDENCY INJECTION
        // ========================================================================

        /**
         * @brief Setzt den Zeiger auf die Dateiverwaltungskomponente der Kalibrierung.
         * @param calibManager Zeiger auf das I_CalibrationManager-Interface.
         */
        void setCalibrationManager(I_CalibrationManager* calibManager) override{ m_calibManager = calibManager; }

        /**
         * @brief Registriert den internen EventManager in der Logik-Schaltzentrale.
         */
        void setEventManager(EventManager* eventManager) { m_eventManager = eventManager; }

        /**
         * @brief Wechselt den aktiven Schicht-1-Zustand der FSM (Uebernahme des nextState-Zeigers).
         */
        void transitionTo(BaseState* nextState);

        // ========================================================================
        // IMPLEMENTIERUNG DER I_LOGIK INTERFACE-METHODEN
        // ========================================================================

        /**
         * @brief Initialisiert die Logik-Komponente mit der physischen Position der Anlage.
         */
        void init(AnlagenPosition pos) override;

        /**
         * @brief Nimmt ein Ereignis von der HAL oder dem Framework entgegen und triggert die Haupt-FSM.
         */
        void processEvent(logik::Event e) override;

        /**
         * @brief Übergibt den von der HAL (Analog-Auswertung) ermittelten Typ eines Werkstücks.
         */
        void workpieceTypeFromHS(WpType type) override;

        /**
         * @brief Verarbeitet Werkstück-Informationen (ID und Typ), die via QNX-Netzwerk empfangen wurden.
         */
        void workpieceInfo(int id, WpType type) override;

        /**
         * @brief Aktualisiert das Rezept der internen Werkstück-Liste nach einer Manipulation.
         */
        void updateWpListe(WpType type) override;

        /**
         * @brief Der zentrale 10ms-Herzschlag des Bandes für interne Wegstrecken-Timer und Watchdogs.
         * @details Ruft stur in jeder Periode m_wpManager->takt() sowie m_errorHandler->takt() auf.
         */
        void takt() override;

        /**
         * @brief Aktualisiert den Bereitschafts- bzw. Füllstandstatus der Rutsche der Partneranlage.
         */
        void setPartnerChuteStatus(bool isFull) override;

        /**
         * @brief Läuft beim Booten und koordiniert das Laden sowie Verteilen der Kalibrierung.
         */
        void loadCalibrationData();

        /**
         * @brief Übergibt die im Service-Modus live gemessenen ADC-Kalibrierungswerte.
         */
        void setLiveAdcCalibration(uint16_t emptyBelt, uint16_t flatCenter, uint32_t minSamples) override;

        bool isEstopSafetyActive() const override;

        // ========================================================================
        // ZUSÄTZLICHE SERVICE-METHODEN
        // ========================================================================

        /**
         * @brief Triggert die mathematische Hochrechnung der Slow-Grenzwerte manuell.
         */
        void recalculateSlowLimits();

        /**
         * @brief Holt die aktuellen Live-Daten und triggert das permanente Speichern über den CalibManager.
         */
        void saveCurrentCalibration();

        // ========================================================================
        // GETTER-METHODEN FÜR COMPONENT-SHARING
        // ========================================================================

        I_Dispatcher* getDispatcher() const { return m_dispatcher; }

        CalibrationData& getCalibrationData() { return m_calibData; }

        // ========================================================================
        // GETTER FÜR DIE GESCHWISTER-KOMPONENTEN (NEU/ERWEITERT)
        // ========================================================================

        /**
         * @brief Gibt einen unbesessenen Zeiger auf das Schieberegister heraus.
         * @return Zeiger auf den WorkpieceManager.
         */
        WorkpieceManager* getWorkpieceManager() const { return m_wpManager.get(); }

        /**
         * @brief Gibt einen unbesessenen Zeiger auf den EventManager heraus.
         * @return Zeiger auf den EventManager.
         */
        EventManager* getEventManager() const { return m_eventManager; }

        /**
         * @brief Gibt einen unbesessenen Zeiger auf den zentralen ErrorHandler heraus.
         * @note KORREKTUR: Ermöglicht dem WorkpieceManager das Absetzen von handleEscalation().
         * @return Zeiger auf den ErrorHandler.
         */
        ErrorHandler* getErrorHandler() const { return m_errorHandler.get(); }

        /**
        * @brief Prueft beim Systemstart, ob die Kalibrierungsdatei existiert.
        */
        void checkCalibrationFile();
    };

} // namespace logik

#endif // LOGIK_HPP
