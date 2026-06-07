/**
 * @file WorkpieceManager.hpp
 * @brief Schieberegister zur logischen Verwaltung aller Werkstücke auf dem Förderband.
 *
 * @details Diese Komponente überwacht die Positionen der Werkstücke mittels eines
 * 10ms-Zeittakt-Modells. Sie berechnet dynamisch Reststrecken bei Geschwindigkeitswechseln
 * und führt die Sortierentscheidung anhand der TargetSequence durch.
 *
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef WORKPIECE_MANAGER_HPP
#define WORKPIECE_MANAGER_HPP

#include <vector>
#include <iostream>
#include "workpiece.hpp"
#include "targetSequence.hpp"
#include "shared/types.hpp"
#include "shared/logikEvent.hpp"

namespace logik {

    class Logik; // Vorwärtsdeklaration des Logik-Kontextes (Schicht 1 FSM)

    class WorkpieceManager {
    private:
        Logik* m_context;                           ///< Zeiger auf den Hauptkontext für Event-Feuerungen & Eskalationen
        std::vector<Workpiece> m_activeWorkpieces;  ///< Das aktive Schieberegister (Digitaler Zwilling)
        TargetSequence m_sequence;                  ///< Das zyklische Sortier-Rezept
        CalibrationData m_calibData;                ///< Geladene Zeit-Kalibrierungsdaten (aus types.hpp)
        double m_speedFactor;                       ///< Berechneter Proportionalitätsfaktor (Slow vs. Normal)

        // ====================================================================
        // ERWEITERUNG FÜR ANLAGEN-POSITIONIERUNG & BEREITSCHAFT
        // ====================================================================
        AnlagenPosition m_myPosition;               ///< Speichert, ob dieser Manager auf Anlage 1 (VORN) oder Anlage 2 (HINTEN) läuft
        bool m_isAnlage2Ready;                      ///< Flag: Ist Anlage 2 bereit zur Materialaufnahme?

    public:
        /**
         * @brief Konstruktor.
         * @param context Zeiger auf die übergeordnete Steuerungskomponente.
         */
        explicit WorkpieceManager(Logik* context);
        ~WorkpieceManager() = default;

        /**
         * @brief Fügt ein neues Werkstück lokal am Einlauf hinzu (Anlage 1).
         * @note Wird über das Event ENTRY_BLOCKED aus der Hauptsteuerung getriggert.
         */
        void addWorkpiece();

        /**
         * @brief Fügt ein Werkstück mit bereits bekannter ID und Typ am Einlauf hinzu (Anlage 2).
         * @note Wird über die i_Logik-Schnittstelle workpieceInfo() via QNX-Netzwerk aufgerufen.
         * @param id Die von Anlage 1 vergebene, eindeutige Werkstück-ID.
         * @param type Der bereits auf Anlage 1 gemessene Werkstücktyp.
         */
        void addNetworkWorkpiece(int id, WpType type);

        /**
         * @brief Der zentrale Herzschlag des Schieberegisters (Aufruf stur alle 10ms via Hardware-Timer).
         * @param isMotorSlow Aktueller Zustand der Bandgeschwindigkeit aus der Haupt-FSM.
         */
        void takt(bool isMotorSlow);

        /**
         * @brief Verarbeitet die hardwarenahen Werkstück-Ereignisse der HAL.
         * @details Sucht das passende Werkstück für das eingetroffene Event. Findet sie kein
         * passendes Teil im Transit, wird dies als unerwartete Manipulation gewertet und ein ERROR eskaliert.
         * @param e Das von der Logikschicht durchgereichte Sensor-Event.
         * @param typeFromHal Der von der HAL ermittelte Typ (exklusiv relevant bei HEIGHT_CLEARED).
         */
        void handleSensorSignal(logik::Event e, WpType typeFromHal = WpType::UNKNOWN);

        /**
         * @brief Setzt die Kalibrierungsdaten und berechnet automatisch den anlagenspezifischen Geschwindigkeitsfaktor.
         */
        void setCalibrationData(const CalibrationData& data);


        /**
         * @brief Liefert das älteste (vorderste) Werkstück an einer bestimmten Position.
         * @return Zeiger auf das gefundene Werkstück, oder nullptr falls kein Teil in dieser Zone existiert.
         */
        Workpiece* getFirstWorkpieceAt(WpLocation loc);

        /**
         * @brief Löscht alle Werkstücke und setzt die Rezeptur zurück (bei STOP, E-STOP oder RESET).
         */
        void clearAll();

        // ====================================================================
        // GETTER- UND SETTER-METHODEN FÜR DIE LOGIK-SCHICHT
        // ====================================================================
        /**
         * @brief Liefert eine Referenz auf die interne Rezeptur-Sequenz.
         * @note Essentiell für Anlage 1, um bei updateWpListe() die forceType()-Methode aufzurufen.
         * @return Referenz auf das TargetSequence-Objekt.
         */
        TargetSequence& getSequence() { return m_sequence; }

        /**
         * @brief Setzt fest, auf welcher Teilanlage dieser Manager instanziiert ist.
         * @note Wird idealerweise über die i_Logik-Methode init(AnlagenPosition pos) übergeben.
         * @param pos Die zugewiesene Anlagenposition (VORN_ANLAGE_1 oder HINTEN_ANLAGE_2).
         */
        void setAnlagenPosition(AnlagenPosition pos) { m_myPosition = pos; }

        /**
         * @brief Aktualisiert den Bereitschaftsstatus von Anlage 2 (wird über QNX-Netzwerk empfangen).
         * @param ready true wenn Anlage 2 bereit ist, false wenn sie blockiert/gestoppt ist.
         */
        void setAnlage2Ready(bool ready) {
            m_isAnlage2Ready = ready;
            std::cout << "[WP-Manager] Status Anlage 2 aktualisiert: " << (ready ? "BEREIT" : "BLOCKIERT") << std::endl;
        }

    private:
        /**
         * @brief Hilfsmethode zum sicheren Entfernen eines Werkstücks über seine eindeutige ID.
         */
        void removeWorkpiece(int id);

        /**
         * @brief Prüft den Werkstücktyp gegen das Rezept, speichert das Flag im WP und schaltet die Weiche.
         * @param wp Zeiger auf das zu prüfende Werkstück an der Weiche.
         */
        void checkAndSetJunction(Workpiece* wp);
    };

} // namespace logik

#endif // WORKPIECE_MANAGER_HPP
