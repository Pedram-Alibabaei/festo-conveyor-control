/**
 * @file Logik.hpp
 * @brief Konkrete Implementierung der zentralen Steuerungskomponente (Zustandsmaschine).
 *
 * @details Diese Klasse verknüpft die übergeordneten Schicht-1-Zustände der Festo-Anlage
 * mit dem zeittaktbasierten Schieberegister (WorkpieceManager). Sie implementiert das
 * Interface i_Logik für die netzwerkweite Kommunikation.
 *
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef LOGIK_HPP
#define LOGIK_HPP

#include "i_Logik.hpp"
#include "workpieceManager/WorkpieceManager.hpp"
#include "dispatcher/dispatcher.hpp" // Zeiger auf euer Netzwerk-/Hardware-Interface
#include <memory>

namespace logik {

    class Logik : public i_Logik {
    private:
        // --- Infrastruktur-Komponenten ---
        std::unique_ptr<WorkpieceManager> m_wpManager; ///< Das zeittaktbasierte Schieberegister (Digitaler Zwilling)
        i_Dispatcher* m_dispatcher;                    ///< Zeiger auf den Dispatcher für HW-Ausgaben und Netzwerknachrichten

        // --- Interne Zustandsvariablen ---
        AnlagenPosition m_anlagenPosition;             ///< Die dieser Instanz zugewiesene physische Position (Anlage 1 oder 2)
        bool m_isPartnerChuteFull;                     ///< Statusflag für die Rutsche der jeweils anderen Teilanlage

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
        // IMPLEMENTIERUNG DER I_LOGIK INTERFACE-METHODEN
        // ========================================================================

        /**
         * @brief Initialisiert die Logik-Komponente mit der physischen Position der Anlage.
         * @details Weist die Rolle (Master/Slave) dynamisch zu und konfiguriert den WorkpieceManager.
         * @param pos Die Position im Gesamtsystem (VORN_ANLAGE_1 oder HINTEN_ANLAGE_2).
         */
        virtual void init(AnlagenPosition pos) override;

        /**
         * @brief Nimmt ein Ereignis von der HAL oder dem Framework entgegen und triggert die Haupt-FSM.
         * @param e Das aufgetretene Ereignis (z.B. logik::Event::HEIGHT_BLOCKED).
         */
        virtual void processEvent(logik::Event e) override;

        /**
         * @brief Übergibt den von der HAL (Analog-Auswertung) ermittelten Typ eines Werkstücks.
         * @details Reicht die Daten direkt an das Schieberegister weiter, um das Transit-Teil zu typisieren.
         * @param type Der erkannte physische Werkstücktyp.
         */
        virtual void workpieceTypeFromHS(WpType type) override;

        /**
         * @brief Verarbeitet Werkstück-Informationen (ID und Typ), die via QNX-Netzwerk empfangen wurden.
         * @details Wird exklusiv auf Anlage 2 aufgerufen, um ein registriertes Teil am Einlauf zu erzeugen.
         * @param id Die von Anlage 1 vergebene, eindeutige Werkstück-ID.
         * @param type Der bereits auf Anlage 1 vor-analysierte Werkstücktyp.
         */
        virtual void workpieceInfo(int id, WpType type) override;

        /**
         * @brief Aktualisiert das Rezept der internen Werkstück-Liste nach einer Manipulation.
         * @details Wird von Anlage 2 über das Netzwerk aufgerufen, wenn dort eine Vertauschung erkannt wurde.
         * Zwingt das Rezept von Anlage 1 absolut zurück auf den fehlerhaften Soll-Typen.
         * @param type Der Typ des Werkstücks, auf den die Rezeptur synchronisiert werden muss.
         */
        virtual void updateWpListe(WpType type) override;

        /**
         * @brief Der zentrale 10ms-Herzschlag des Bandes für interne Wegstrecken-Timer und Watchdogs.
         * @details Ruft stur in jeder Periode m_wpManager->takt() auf.
         */
        virtual void takt() override;

        /**
         * @brief Aktualisiert den Bereitschafts- bzw. Füllstandstatus der Rutsche der Partneranlage.
         * @param isFull true, wenn die Rutsche blockiert/voll ist, andernfalls false.
         */
        virtual void setPartnerChuteStatus(bool isFull) override;

        /**
         * @brief Übergibt die vollständigen Kalibrierungsdaten als Paket an die Logik.
         * @details Schiebt das gesamte Struct direkt in den WorkpieceManager, damit dort
         * die dynamischen Grenzwerte berechnet werden können.
         * @param data Das geladene und vollständig befüllte CalibrationData-Struct.
         */
        virtual void setCalibrationData(const CalibrationData& data) override;

        // ========================================================================
        // GETTER-METHODEN FÜR COMPONENT-SHARING
        // ========================================================================

        /**
         * @brief Liefert den Zeiger auf das zugewiesene Dispatcher-Interface.
         * @return Zeiger auf das i_Dispatcher-Objekt.
         */
        i_Dispatcher* getDispatcher() const { return m_dispatcher; }
    };

} // namespace logik

#endif // LOGIK_HPP
