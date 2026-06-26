/**
 * @file WorkpieceManager.hpp
 * @brief Schieberegister zur logischen Verwaltung aller Werkstücke auf dem Förderband.
 *
 * SoSe2026 Team 3.3
 */

#ifndef WORKPIECE_MANAGER_HPP
#define WORKPIECE_MANAGER_HPP

#include <vector>
#include "Workpiece.hpp"
#include "targetSequence.hpp"
#include "shared/types.hpp"
#include "shared/logikEvent.hpp"
#include "shared/detailLog.hpp"

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

        // Netzwerk-Uebergabe Anlage 1 -> Anlage 2
        bool m_awaitingHandoverReady;               ///< Anlage 1: END_BLOCKED, wartet auf READY_TO_RECEIVE
        bool m_handoverInfoSent;                    ///< Anlage 1: WP-Info gesendet, END_CLEARED erlaubt
        bool m_handoverMotorActive;                 ///< Anlage 1: Motor laeuft bis Partner ENTRY_BLOCKED meldet
        bool m_handoverReceiveActive;               ///< Anlage 2: WP-Info erhalten, wartet auf ENTRY_BLOCKED
        bool m_hasPendingNetworkWp;                  ///< Anlage 2: WP-Info erhalten, wartet auf ENTRY_BLOCKED
        int m_pendingNetworkWpId;
        WpType m_pendingNetworkWpType;
        int m_handoverRetryTicks;                   ///< Anlage 1: Takte bis zum naechsten REQ_TO_GIVE-Retry
        int m_handoverReceiveTicks;                 ///< Anlage 2: Takte seit WP-Info bis ENTRY_BLOCKED
        bool m_handoverPausedByPartner;             ///< Anlage 1: Uebergabe pausiert, weil A2 Bandende blockiert

        // ====================================================================
        // TAKT-BASIERTE AKTOR-ZEITSTEUERUNG
        // ====================================================================
        bool m_isSwitchOpen;                        ///< Zustandstracking, ob die Weiche gerade offen ist
        int m_switchOpenTicks;                      ///< Zaehlt die verbleibenden 10ms-Takte fuer das Offenhalten
        bool m_isEjectorPushed;                     ///< Zustandstracking, ob der Auswerfer gerade ausgefahren ist
        int m_ejectorPushTicks;                     ///< Zaehlt die verbleibenden 10ms-Takte fuer das Auswerfen
        int m_metalWaitWpId;                        ///< Auswerfer: WP wartet auf Metall-Sensor vor Sortierentscheid
        int m_metalWaitTicks;                       ///< Rest-Takte bis Entscheid ohne Metall-Signal
        int m_pendingMetalDecisionWpId;             ///< WP an Sortiereinheit wartet noch auf Metall-Entscheidung
        bool m_ejectorLsBlocked;                    ///< Sortierweichen-LS zuletzt blockiert (fuer spaete CLEARED-Flanken)
        int m_ejectorBlockedWpId;                   ///< WP, das zuletzt EJECTOR_BLOCKED ausgeloest hat (-1 = keins)
        bool m_pendingEjectorBlockedAfterHeight;    ///< LS an Weiche kam kurz vor HEIGHT_CLEARED und wird nachgezogen
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

        /** Weiche/Auswerfer-Ruecksetzung – laeuft auch bei Motor aus. */
        void taktActuators();

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
        const Workpiece* getFirstWorkpieceAt(WpLocation loc) const;

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

        /** Metall-Warte-Timer zuruecksetzen (z. B. bei BOTH_CHUTES_FULL). */
        void resetMetalWaitState();

        // ====================================================================
        // GETTER- UND SETTER-METHODEN FÜR DIE LOGIK-SCHICHT
        // ====================================================================
        /** Nach Motor Slow/Fast: Limits aller WPs in Transit-Zonen anpassen. */
        void onMotorSpeedChanged(bool isMotorSlow);

        TargetSequence& getSequence() { return m_sequence; }
        void setAnlagenPosition(AnlagenPosition pos) { m_myPosition = pos; }

        void setAnlage2Ready(bool ready) {
            m_isAnlage2Ready = ready;
            DETAIL_LOG("[WP-Manager] Status Anlage 2 aktualisiert: " << (ready ? "BEREIT" : "BLOCKIERT"));
        }

       bool isAnlage2Ready(){
         return m_isAnlage2Ready;
       }

       bool hasActiveWorkpieces() const { return !m_activeWorkpieces.empty(); }

       /** Anlage 2: Kapazitaet fuer weiteres WP (max. 2 inkl. ausstehende Uebergabe). */
       bool canAcceptMoreWorkpiecesOnAnlage2() const;

       /** Noch WPs auf dem Band, die Förderband-Bewegung brauchen? */
       bool hasWorkpiecesNeedingMotor() const;

       /** Einlauf-WP wartet regulaer hinter fuehrendem WP auf dem Band? */
       bool isEntryQueuedBehindLeadingWorkpiece() const;

       /** Fuehrendes WP hat den Hoehensensor noch nicht erreicht (AT_ENTRY oder TRANSIT_TO_HEIGHT)? */
       bool hasLeadingWorkpieceBeforeHeightSensor() const;

       /** Anlage 2: ENTRY_BLOCKED erwartet wegen Netzwerk-Uebergabe (pending, Einlauf leer)? */
       bool isExpectedNetworkEntryAtAnlage2() const;

       /** Echter Einlauf-Fehler: zweites Teil / belegter Einlauf, nicht nur LS-Stoerung im Transit. */
       bool isEarlyEntryViolationOnBlocked() const;

       /** Motor darf stoppen: keine aktiven Transit-/Entry-WPs mehr. */
       bool shouldAllowMotorStop() const { return !hasWorkpiecesNeedingMotor(); }

       bool hasPendingNetworkWorkpiece() const { return m_hasPendingNetworkWp; }

       /** Anlage 2: REQ_TO_GIVE vom Partner – bereit antworten? */
       void onPartnerHandoverRequest();

       /** Anlage 1: READY_TO_RECEIVE – WP-Info senden und Band an. */
       void onPartnerReadyToReceive();

       /** Anlage 1: Partner meldet WP am Einlauf -> Motor aus. */
       void onPartnerHandoverComplete();

       /** Anlage 1: Partner meldet WP verloren bei Uebergabe -> Motor aus. */
       void onPartnerHandoverWpLost();

       /** Anlage 1: Partner blockiert waehrend Uebergabe -> Motor pausieren. */
       void onPartnerHandoverPause();

       /** Anlage 2: WP-Info zwischenspeichern bis ENTRY_BLOCKED. */
       void storePendingNetworkWorkpiece(int id, WpType type);

       /** Anlage 2: Pending-WP physisch am Einlauf -> in Liste. */
       void commitPendingNetworkWorkpieceAtEntry();

       /** Handover-Flags zuruecksetzen (clearAll, Fehler). */
       void resetHandoverState();

       /** Anlage 1: Motor laeuft fuer laufende Netzwerk-Uebergabe. */
       bool isHandoverMotorActive() const { return m_handoverMotorActive; }

       /** Anlage 1: WP wartet am Bandende auf READY_TO_RECEIVE vom Partner. */
       bool isAwaitingHandoverReady() const { return m_awaitingHandoverReady; }

       /** Anlage 2: Wartet auf physisches ENTRY_BLOCKED nach WP-Info. */
       bool isHandoverReceiveActive() const { return m_handoverReceiveActive; }

       /** Anlage 2: Partner ueber fehlgeschlagene Uebergabe informieren. */
       void notifyPartnerHandoverFailed();

       /** Anlage 2: Rezeptur an Anlage 1 synchronisieren (UPDATE_LIST). */
       void notifyPartnerSequenceSync(WpType type = WpType::UNKNOWN);

       /** Anlage 2: Nach Fehlerende ggf. READY_TO_RECEIVE erneut an Anlage 1 senden. */
       void recoverPartnerHandoverAfterError();

       /** Slow-Motor noetig fuer mindestens ein aktives WP (Error-Rueckkehr). */
       bool shouldUseSlowMotor() const;

    private:
        bool canAcceptNetworkHandover();

        /**
         * @brief Prüft den Werkstücktyp gegen das Rezept, speichert das Flag im WP und schaltet die Weiche.
         */
        void checkAndSetJunction(Workpiece* wp);

        /**
         * @brief Aussortierung mit Rutsche-voll-Prüfung (lokal, ggf. Partner).
         */
        void handleSortOutDecision(Workpiece* wp);

        /** Rutsche blockiert waehrend Aussortierung – WARNING oder BOTH_CHUTES_FULL. */
        void handleChuteTransitFailure(Workpiece& wp);

        /** Mehr-WP: Nachfolgendes WP an Sortier-LS nachziehen, wenn dessen Flanke mit dem fuehrenden WP zusammenfiel. */
        bool promoteTrailingWorkpieceAtSorterIfDue(const char* reason);

        /** Anlage 1: REQ_TO_GIVE wiederholen, solange WP am Bandende wartet. */
        void retryPendingHandoverRequest();

        /** Anlage 2: Bandende frei -> Anlage 1 informieren (READY_TO_RECEIVE). */
        void notifyPartnerHandoverSlotFree();

        std::uint32_t scaleNormalToSlowTicks(std::uint32_t normalTicks) const;
        /** 6,9-cm-Worst-Case in Normal-Takten (fest aus ticks_EntryB_to_EntryC_Normal). */
        std::uint32_t calculateCalibratedWorstCaseExtraNormal() const;
        /**
         * Limit EntryC -> HeightB.
         * @param afterSetSlow false = Fast-Phase, true = Mixed/Slow nach SET_SLOW.
         */
        std::uint32_t calculateTransitToHeightLimit(bool afterSetSlow) const;
        /** Konservatives Limit Hoehenmessung (HeightB -> HeightC, Slow-Kalibrierung). */
        std::uint32_t calculateHeightMeasurementLimit() const;
        /** Limit WP an Sortierweiche (EjectorB -> EjectorC, Slow-Kalibrierung). */
        std::uint32_t calculateEjectorOccupancyLimit() const;
        /** Konservatives Limit HS -> Sortierweiche (Normal, Slow-Kalibrierung, skaliert). */
        std::uint32_t calculateEjectorTransitLimit() const;
        /** Limit Sortierweiche -> Bandende (Normal bei Fast, skaliert bei Slow). */
        std::uint32_t calculateEndTransitLimit(bool isMotorSlow) const;
        void updateTargetLimitForMotor(Workpiece& wp, bool isMotorSlow) const;

        Workpiece* findWorkpieceById(int id);
        bool hasEjectorSorter() const;
        bool shouldDeferJunctionForMetal(const Workpiece* wp) const;
        void applyMetalTypeUpgrade(Workpiece* wp);
        void applyMetalIfDetected(Workpiece* wp);
        Workpiece* getLeadingWorkpieceForMetalDetection();
        void startSorterDecisionForWp(Workpiece* wp);
        /** Rezept nach WP_LOST auf dem Weg zum Bandende wieder auf den verlorenen Typ setzen. */
        void syncSequenceAfterLostTransitToEnd(const Workpiece& wp);

        /** Rezept vorruecken; auf Anlage 2 Bediener-Hinweis fuer naechstes Soll-WP. */
        void advanceTargetSequence();
    };

} // namespace logik

#endif // WORKPIECE_MANAGER_HPP
