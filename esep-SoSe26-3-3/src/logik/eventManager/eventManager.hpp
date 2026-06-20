/**
 * @file eventManager.hpp
 * @brief Zentraler Orchestrator zwischen Dispatcher-Events und Fachmodulen.
 *
 * Rolle des EventManagers (Mediator + PlantContext):
 *  - PlantContext: Laufzeit-Flags der Anlage (Motor, Estop, Fehler, Einlauf, …)
 *  - Vorverarbeitung: Roh-Events → FSM-taugliche Signale (z.B. START kurz/lang)
 *  - Routing: leitet Events an ErrorHandler, WorkpieceManager und FSM weiter
 *  - Takt-Gate: entscheidet, ob der WorkpieceManager-Takt laufen darf
 *
 * Der EventManager ist KEIN Ersatz für die FSM:
 *  Zustandswechsel (Idle/Running/Service/Estop) bleiben in den FSM-States.
 *
 * SoSe2026 Team 3.3
 */

#ifndef EVENT_MANAGER_HPP
#define EVENT_MANAGER_HPP

#include "shared/types.hpp"
#include "shared/logikEvent.hpp"
#include <chrono>
#include <memory>
#include <vector>

namespace logik {

class Logik;
class WorkpieceManager;
class ErrorHandler;
class Fsm;
class BaseState;

/**
 * @brief Laufzeit-Kontext der Anlage — von FSM-States und Subsystemen gelesen/gesetzt.
 */
struct PlantContext {
    bool motorOn = false;
    bool motorSlow = false;
    bool eStopActive = false;
    bool errorActive = false;
    bool entryFrei = true;
    bool chuteFull = false;
    bool partnerChuteFull = false;
    bool endBlocked = false;
    bool serviceMode = false;
    bool connectionLost = false;
    bool calibrationDataReady = false;
    bool partnerCalibrationReady = false;
    AnlagenPosition position = AnlagenPosition::UNKNOWN_POSITION;
};

class EventManager {
private:
    Logik* m_context;

    WorkpieceManager* m_wpManager;
    ErrorHandler* m_errorHandler;
    std::unique_ptr<Fsm> m_fsm;

    PlantContext m_ctx;

    std::chrono::steady_clock::time_point m_startPressedStartTime;
    std::chrono::steady_clock::time_point m_partnerStartPressedTime;
    bool m_startButtonHeld = false;
    bool m_entryDeferredWhileEndBlocked = false;
    /** Anlage 1: ENTRY_BLOCKED kam, waehrend Einlauf noch gesperrt (voriges WP unterwegs). */
    bool m_entryDeferredWhileOccupied = false;

    bool canRouteToBandSubsystems() const;
    bool isFsmPassthroughEvent(Event e) const;

    void updateContextFlags(Event e);
    void routeToErrorHandler(Event e);
    void routeToWorkpieceManager(Event e);
    void routeToFsm(Event e);
    std::vector<Event> preprocessForFsm(Event e);
    void resumeEntryAfterEndCleared(std::vector<Event>& fsmEvents);
    void resumeDeferredEntryWhileOccupied(std::vector<Event>& fsmEvents);

public:
    explicit EventManager(Logik* context);
    ~EventManager();

    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;

    void init(AnlagenPosition pos);

    /** Einstiegspunkt fuer alle Events aus Logik/Dispatcher. */
    void handleEvent(Event e);

    /** 10ms-Bandtakt — WorkpieceManager nur bei freigegebenem Betrieb. */
    void handleTakt();

    void transitionFsmState(std::unique_ptr<BaseState> nextState);

    const PlantContext& context() const { return m_ctx; }

    // --- Kompatibilitaets-Getter/Setter (delegieren an m_ctx) ---
    void setMotorOn(bool on) { m_ctx.motorOn = on; }
    bool isMotorOn() const { return m_ctx.motorOn; }

    void setMotorSlow(bool slow) { m_ctx.motorSlow = slow; }
    bool isMotorSlow() const { return m_ctx.motorSlow; }

    void setEndBlocked(bool blocked) { m_ctx.endBlocked = blocked; }
    bool isEndBlocked() const { return m_ctx.endBlocked; }

    void setEStopActive(bool active) { m_ctx.eStopActive = active; }
    bool isEStopActive() const { return m_ctx.eStopActive; }

    void setErrorActive(bool active) { m_ctx.errorActive = active; }
    bool isErrorActive() const { return m_ctx.errorActive; }

    void setEntryFrei(bool frei) { m_ctx.entryFrei = frei; }
    bool isEntryFrei() const { return m_ctx.entryFrei; }

    void setChuteFull(bool isFull) { m_ctx.chuteFull = isFull; }
    bool isChuteFull() const { return m_ctx.chuteFull; }

    void setPartnerChuteStatus(bool isFull);
    bool isPartnerChuteFull() const { return m_ctx.partnerChuteFull; }

    void setServiceMode(bool active) { m_ctx.serviceMode = active; }
    bool isServiceMode() const { return m_ctx.serviceMode; }

    void setConnectionLost(bool lost) { m_ctx.connectionLost = lost; }
    bool isConnectionLost() const { return m_ctx.connectionLost; }

    void setCalibrationDataReady(bool ready) { m_ctx.calibrationDataReady = ready; }
    bool isCalibrationDataReady() const { return m_ctx.calibrationDataReady; }

    void setPartnerCalibrationReady(bool ready) { m_ctx.partnerCalibrationReady = ready; }
    bool isPartnerCalibrationReady() const { return m_ctx.partnerCalibrationReady; }

    /** START-Druck-Baseline fuer kurz/lang-Erkennung zuruecksetzen. */
    void resetStartPressTimer();

    bool isStartButtonHeld() const { return m_startButtonHeld; }
};

} // namespace logik

#endif // EVENT_MANAGER_HPP
