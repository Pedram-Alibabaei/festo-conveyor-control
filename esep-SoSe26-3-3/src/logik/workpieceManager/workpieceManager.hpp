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
        bool m_partnerReadyDeferred;                ///< Anlage 1: READY empfangen, Uebergabe bei Running starten
        bool m_handoverPauseResumeDeferred;         ///< Anlage 1: pausierte Uebergabe bei Running fortsetzen
        bool m_partnerHandoverRequestPending;       ///< Anlage 2: REQ waehrend Idle/ReadyForRun -> bei Running beantworten
        int m_lastHandoverWpId;                     ///< Anlage 1: zuletzt uebergebenes WP (fuer Bediener-Log)
        WpType m_lastHandoverWpType;                ///< Anlage 1: Typ des zuletzt uebergebenen WPs

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
        int m_sorterEchoTicks;                      ///< Nach Weiche/Auswerfer-Bewegung: LS-Echos ignorieren (10ms-Takte)
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

        /** Entfernt WP am Einlauf, wenn die LS frei ist (Geister-Eintrag ohne physisches Teil). */
        bool discardGhostEntryWorkpieceIfSensorClear();

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

       /** Running-Eintritt: Motor nur bei Transit-WPs oder ausstehender Netzwerk-Annahme (A2). */
       bool shouldResumeMotorOnRunningEntry() const;

       /** Anlage 1: WP am Bandende blockiert Motorstart (Uebergabe/END_BLOCKED). */
       bool isAnlage1BandEndHandoverPending() const;

       /** Anlage 1: Partner-HANDOVER_PAUSE – Motor bis READY/Fortsetzung aus. */
       bool isAnlage1MotorPausedForPartnerHandover() const;

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

       /** Weiche/Auswerfer bewegt sich oder kurze Nachlaufzeit – EJECTOR-LS kann ohne WP ausloesen. */
       bool isSorterEchoWindowActive() const;

       bool hasPendingNetworkWorkpiece() const { return m_hasPendingNetworkWp; }

       /** Anlage 2: REQ_TO_GIVE vom Partner – bereit antworten? */
       void onPartnerHandoverRequest();

       /** Anlage 2: Running verlassen – laufende Uebergabe pausieren (kein WP_LOST). */
       void pauseHandoverWhileNotRunning();

       /** Anlage 2: Running wieder aktiv – pending WP-Uebergabe fortsetzen. */
       void resumePendingNetworkHandoverOnRunning();

       /** Anlage 2: Running verlassen – Uebergabe pausieren, ggf. A1 stoppen. */
       void onAnlage2RunningExit();

       /** Anlage 1: Running verlassen – gespeichertes READY verwerfen (Partner erneut anfragen). */
       void onAnlage1RunningExit();

       /** Anlage 2: Nach MOTOR_RUN Timeout fuer pending WP-Info starten. */
       void tryArmPendingNetworkHandover();

       /** Anlage 2: Gespeicherte REQ von Anlage 1 bei Running mit READY beantworten. */
       void resumePartnerHandoverRequestOnRunning();

       /** Anlage 2: Nach END_CLEARED Partner-READY senden (gespeicherte REQ oder freies Band). */
       void completeHandoverAfterEndCleared();

       /** Anlage 1: Gespeicherte READY bei Running mit WP-Info + Motor beantworten. */
       void resumePendingHandoverOnRunning();

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

        /** READY senden wenn Uebergabe moeglich; REQ-Pending-Flag loeschen. */
        void sendPartnerHandoverReadyIfPossible();

        /** Anlage 1: WP-Info senden und Uebergabe-Motor starten (Flags vor MOTOR_RUN setzen). */
        void startAnlage1HandoverTransmission(Workpiece* wp);

        std::uint32_t scaleNormalToSlowTicks(std::uint32_t normalTicks) const;
        std::uint32_t scaleSlowToNormalTicks(std::uint32_t slowTicks,
                                             std::uint32_t segmentNormal,
                                             std::uint32_t segmentSlow) const;
        std::uint32_t resolveSegmentSlowTicks(std::uint32_t segmentNormal,
                                              std::uint32_t segmentSlowCalib) const;
        /**
         * Deadline = ticksElapsed + Reststrecke(in aktueller Motor-Geschwindigkeit) + Safety.
         * @param normalEquivalentConsumed bereits zurueckgelegte Strecke in Normal-Takten
         * @param extraNormal zusaetzlicher Normal-Pfad (z. B. 6,9-cm-Worst-Case)
         */
        std::uint32_t calculateRemainingSegmentLimit(std::uint32_t ticksElapsed,
                                                     std::uint32_t normalEquivalentConsumed,
                                                     std::uint32_t segmentNormal,
                                                     std::uint32_t segmentSlowCalib,
                                                     bool isMotorSlow,
                                                     std::uint32_t safetyTicks,
                                                     std::uint32_t extraNormal = 0) const;
        std::uint32_t getTotalNormalEquivalentConsumed(const Workpiece& wp,
                                                       std::uint32_t segmentNormal,
                                                       std::uint32_t segmentSlowCalib,
                                                       bool elapsedAtSlowMotor) const;
        void flushNormalEquivalentProgress(Workpiece& wp,
                                           std::uint32_t segmentNormal,
                                           std::uint32_t segmentSlowCalib,
                                           bool elapsedAtSlowMotor);
        void applySegmentTargetLimit(Workpiece& wp,
                                     bool isMotorSlow,
                                     std::uint32_t segmentNormal,
                                     std::uint32_t segmentSlowCalib,
                                     std::uint32_t safetyTicks,
                                     std::uint32_t extraNormal = 0) const;
        void recalculateTargetLimitAfterMotorChange(Workpiece& wp,
                                                    bool newMotorSlow,
                                                    std::uint32_t segmentNormal,
                                                    std::uint32_t segmentSlowCalib,
                                                    std::uint32_t safetyTicks,
                                                    std::uint32_t extraNormal = 0);

        std::uint32_t calculateCalibratedWorstCaseExtraNormal() const;
        std::uint32_t calculateEjectorTransitLimit() const;

        Workpiece* findWorkpieceById(int id);
        bool hasEjectorSorter() const;
        bool shouldDeferJunctionForMetal(const Workpiece* wp) const;
        /** STUCK-Timer an Sortier-LS pausieren (Metall-Warte, Weiche/Auswerfer aktiv). */
        bool shouldPauseEjectorOccupancyTimer(const Workpiece& wp) const;
        /** MOTOR_RUN nur wenn kein Fehler aktiv. */
        bool mayRequestMotorRun() const;
        void requestMotorRunIfAllowed(const char* reason);
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
