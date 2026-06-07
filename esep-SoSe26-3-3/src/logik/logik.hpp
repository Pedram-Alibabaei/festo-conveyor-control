/**
 * @file Logik.hpp
 * @brief Konkrete Implementierung der zentralen Steuerungskomponente (Zustandsmaschine).
 *
 * @details Diese Klasse verknüpft die übergeordneten Schicht-1-Zustände der Festo-Anlage
 * mit dem zeittaktbasierten Schieberegister (WorkpieceManager). Sie implementiert das
 * Interface i_Logik für die netzwerkweite Kommunikation.
 *
 */

#ifndef LOGIK_HPP
#define LOGIK_HPP

#include "i_Logik.hpp"
#include "workpieceManager/WorkpieceManager.hpp"
#include "dispatcher/dispatcher.hpp"
#include <memory>

class i_Dispatcher;
class I_CalibrationManager; // Vorwärtsdeklaration des CalibrationManager-Interfaces

namespace logik {

    class Logik : public i_Logik {
    private:
        // --- Infrastruktur-Komponenten ---
        std::unique_ptr<WorkpieceManager> m_wpManager; ///< Das zeittaktbasierte Schieberegister (Digitaler Zwilling)
        i_Dispatcher* m_dispatcher;                    ///< Zeiger auf den Dispatcher für HW-Ausgaben und Netzwerknachrichten
        I_CalibrationManager* m_calibManager;          ///< Zeiger auf den CalibrationManager für Datei-Operationen

        // --- Kalibrierungsdaten (Zentral in der Logik verwaltet) ---
        CalibrationData m_calibData;                   ///< Geladene Zeit-Kalibrierungsdaten (aus types.hpp)
        double m_speedFactor;                          ///< Berechneter Proportionalitätsfaktor (Slow vs. Normal)

        // --- Interne Zustandsvariablen ---
        AnlagenPosition m_anlagenPosition;             ///< Die dieser Instanz zugewiesene physische Position (Anlage 1 oder 2)
        bool m_isPartnerChuteFull;                     ///< Statusflag für die Rutsche der jeweils anderen Teilanlage
        bool m_isMotorSlow;                            ///< KORREKTUR: Als Member-Variable statt global deklariert

    public:
        /**
         * @brief Konstruktor.
         * @param dispatcher Zeiger auf das Kommunikations- und Hardware-Interface.
         */
        explicit Logik(i_Dispatcher* dispatcher);

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
        void setCalibrationManager(I_CalibrationManager* calibManager) { m_calibManager = calibManager; }

        // ========================================================================
        // IMPLEMENTIERUNG DER I_LOGIK INTERFACE-METHODEN
        // ========================================================================

        /**
         * @brief Initialisiert die Logik-Komponente mit der physischen Position der Anlage.
         * @details Weist die Rolle (Master/Slave) dynamisch zu und konfiguriert den WorkpieceManager.
         * @param pos Die Position im Gesamtsystem (VORN_ANLAGE_1 oder HINTEN_ANLAGE_2).
         */
        void init(AnlagenPosition pos) override;

        /**
         * @brief Nimmt ein Ereignis von der HAL oder dem Framework entgegen und triggert die Haupt-FSM.
         * @param e Das aufgetretene Ereignis (z.B. logik::Event::HEIGHT_BLOCKED).
         */
        void processEvent(logik::Event e) override;

        /**
         * @brief Übergibt den von der HAL (Analog-Auswertung) ermittelten Typ eines Werkstücks.
         * @details Reicht die Daten direkt an das Schieberegister weiter, um das Transit-Teil zu typisieren.
         * @param type Der erkannte physische Werkstücktyp.
         */
        void workpieceTypeFromHS(WpType type) override;

        /**
         * @brief Verarbeitet Werkstück-Informationen (ID und Typ), die via QNX-Netzwerk empfangen wurden.
         * @details Wird exklusiv auf Anlage 2 aufgerufen, um ein registriertes Teil am Einlauf zu erzeugen.
         * @param id Die von Anlage 1 vergebene, eindeutige Werkstück-ID.
         * @param type Der bereits auf Anlage 1 vor-analysierte Werkstücktyp.
         */
        void workpieceInfo(int id, WpType type) override;

        /**
         * @brief Aktualisiert das Rezept der internen Werkstück-Liste nach einer Manipulation.
         * @details Wird von Anlage 2 über das Netzwerk aufgerufen, wenn dort eine Vertauschung erkannt wurde.
         * Zwingt das Rezept von Anlage 1 absolut zurück auf den fehlerhaften Soll-Typen.
         * @param type Der Typ des Werkstücks, auf den die Rezeptur synchronisiert werden muss.
         */
        void updateWpListe(WpType type) override;

        /**
         * @brief Der zentrale 10ms-Herzschlag des Bandes für interne Wegstrecken-Timer und Watchdogs.
         * @details Ruft stur in jeder Periode m_wpManager->takt() auf.
         */
        void takt() override;

        /**
         * @brief Aktualisiert den Bereitschafts- bzw. Füllstandstatus der Rutsche der Partneranlage.
         * @param isFull true, wenn die Rutsche blockiert/voll ist, andernfalls false.
         */
        void setPartnerChuteStatus(bool isFull) override;

        /**
         * @brief Läuft beim Booten und koordiniert das Laden sowie Verteilen der Kalibrierung.
         */
        void loadCalibrationData();

        /**
         * @brief Übergibt die im Service-Modus live gemessenen ADC-Kalibrierungswerte.
         * @note Harmonisiert mit dem Interface-Namen i_Logik.
         */
        void setLiveAdcCalibration(uint16_t emptyBelt, uint16_t flatCenter, uint32_t minSamples) override;

        // ========================================================================
        // ZUSÄTZLICHE SERVICE-METHODEN
        // ========================================================================

        /**
         * @brief Triggert die mathematische Hochrechnung der Slow-Grenzwerte manuell.
         * @note Nützlich, wenn im Service-Modus neue Normal-Werte eingelernt wurden.
         */
        void recalculateSlowLimits();

        /**
         * @brief Holt die aktuellen Live-Daten und triggert das permanente Speichern über den CalibManager.
         */
        void saveCurrentCalibration();

        // ========================================================================
        // GETTER-METHODEN FÜR COMPONENT-SHARING
        // ========================================================================

        /**
         * @brief Liefert den Zeiger auf das zugewiesene Dispatcher-Interface.
         * @return Zeiger auf das i_Dispatcher-Objekt.
         */
        i_Dispatcher* getDispatcher() const { return m_dispatcher; }

        CalibrationData* getCalibData() const { return m_calibData; }
    };

} // namespace logik

#endif // LOGIK_HPP