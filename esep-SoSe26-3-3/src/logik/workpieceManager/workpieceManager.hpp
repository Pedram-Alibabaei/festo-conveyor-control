/**
 * @file WorkpieceManager.hpp
 * @brief Schieberegister zur logischen Verwaltung aller Werkstücke auf dem Förderband.
 *
 * SoSe2026 Team 3.3
 */

#ifndef WORKPIECE_MANAGER_HPP
#define WORKPIECE_MANAGER_HPP

#include <vector>
#include "workpiece.hpp"
#include "targetSequence.hpp"
#include "shared/types.hpp"
#include "shared/logikEvent.hpp"

namespace logik {

    class Logik;

    class WorkpieceManager {
    private:
        Logik* m_context;                           ///< Zeiger auf den Hauptkontext für Event-Feuerungen & Eskalationen
        std::vector<Workpiece> m_activeWorkpieces;  ///< Das aktive Schieberegister (Digitaler Zwilling)
        TargetSequence m_sequence;                  ///< Das zyklische Sortier-Rezept
        CalibrationData m_calibData;                ///< Geladene Zeit-Kalibrierungsdaten (aus types.hpp)
        double m_speedFactor;                       ///< Berechneter Proportionalitätsfaktor (Slow vs. Normal)

        // ====================================================================
        // ANLAGEN-POSITIONIERUNG & BEREITSCHAFT
        // ====================================================================
        AnlagenPosition m_myPosition;               ///< Speichert, ob dieser Manager auf Anlage 1 (VORN) oder Anlage 2 (HINTEN) läuft
        bool m_isAnlage2Ready;                      ///< Flag: Ist Anlage 2 bereit zur Materialaufnahme?

        // ====================================================================
        // TAKT-BASIERTE AKTOR-ZEITSTEUERUNG
        // ====================================================================
        bool m_isSwitchOpen;                        ///< Zustandstracking, ob die Weiche gerade offen ist
        int m_switchOpenTicks;                      ///< Zaehlt die verbleibenden 10ms-Takte fuer das Offenhalten
        bool m_isEjectorPushed;                     ///< Zustandstracking, ob der Auswerfer gerade ausgefahren ist
        int m_ejectorPushTicks;                     ///< Zaehlt die verbleibenden 10ms-Takte fuer das Auswerfen
        const int TICKS_ONE_SECOND;                 ///< Konstante: 100 Takte * 10ms = 1000ms (1 Sekunde)

    public:
        /**
         * @brief Konstruktor.
         * @param context Zeiger auf die übergeordnete Steuerungskomponente.
         */
        explicit WorkpieceManager(Logik* context);
        ~WorkpieceManager() = default;

        /**
         * @brief Fügt ein neues Werkstück lokal am Einlauf hinzu (Anlage 1).
         */
        void addWorkpiece();

        /**
         * @brief Fügt ein Werkstück mit bereits bekannter ID und Typ am Einlauf hinzu (Anlage 2).
         */
        void addNetworkWorkpiece(int id, WpType type);

        /**
         * @brief Der zentrale Herzschlag des Schieberegisters (Aufruf stur alle 10ms via Hardware-Timer).
         */
        void takt(bool isMotorSlow);

        /**
         * @brief Verarbeitet die hardwarenahen Werkstück-Ereignisse der HAL.
         */
        void handleSensorSignal(logik::Event e, WpType typeFromHal = WpType::UNKNOWN);

        /**
         * @brief Setzt die Kalibrierungsdaten und berechnet automatisch den anlagenspezifischen Geschwindigkeitsfaktor.
         */
        void setCalibrationData(const CalibrationData& data);

        /**
         * @brief Liefert das älteste (vorderste) Werkstück an einer bestimmten Position.
         */
        Workpiece* getFirstWorkpieceAt(WpLocation loc);

        /**
         * @brief Löscht alle Werkstücke und setzt die Rezeptur zurück (bei STOP, E-STOP oder RESET).
         */
        void clearAll();

        void setSpeedFactor(double factor) {m_speedFactor = factor;}

        // ====================================================================
        // NEU: SORTIER-ENTSCHEIDUNGS-METHODEN
        // ====================================================================
        /**
         * @brief Werkstueck entspricht dem Rezept -> Darf passieren.
         */
        void canPass();

        /**
         * @brief Werkstueck ist fehlerhaft/falsch -> Wird aussortiert.
         */
        void canNotPass();

        // ====================================================================
        // KORREKTUR: JETZT PUBLIC FÜR DEN ERROR-HANDLER
        // ====================================================================
        /**
         * @brief Hilfsmethode zum sicheren Entfernen eines Werkstücks über seine eindeutige ID.
         * @note Von privat nach öffentlich verschoben, damit der ErrorHandler verlorene/steckengebliebene Teile löschen kann.
         * @param id Die ID des zu löschenden Werkstücks.
         */
        void removeWorkpiece(int id);

        // ====================================================================
        // GETTER- UND SETTER-METHODEN FÜR DIE LOGIK-SCHICHT
        // ====================================================================
        TargetSequence& getSequence() { return m_sequence; }
        void setAnlagenPosition(AnlagenPosition pos) { m_myPosition = pos; }

        void setAnlage2Ready(bool ready) {
            m_isAnlage2Ready = ready;
            std::cout << "[WP-Manager] Status Anlage 2 aktualisiert: " << (ready ? "BEREIT" : "BLOCKIERT") << std::endl;
        }

       bool isAnlage2Ready(){
         return m_isAnlage2Ready;
       }

       bool hasActiveWorkpieces() const { return !m_activeWorkpieces.empty(); }

    private:
        /**
         * @brief Prüft den Werkstücktyp gegen das Rezept, speichert das Flag im WP und schaltet die Weiche.
         */
        void checkAndSetJunction(Workpiece* wp);
    };

} // namespace logik

#endif // WORKPIECE_MANAGER_HPP
