/**
 * @file WorkpieceManager.cpp
 * @brief Implementierung des zeittaktbasierten Schieberegisters mit zonenbasierter Fehlereskalation.
 *
 * SoSe2026 Team 3.3
 */

#include "WorkpieceManager.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/errorHandler/errorHandler.hpp"
#include <algorithm>
#include <iostream>
#include "shared/logikEvent.hpp"
#include "shared/detailLog.hpp"
#include "shared/operatorLog.hpp"

int Workpiece::s_next_id = 1;

namespace logik {

namespace {
constexpr double kWorkpieceDiameterCm = 4.0;
constexpr double kWorstCaseExtraPathCm = 6.9;
constexpr double kSetSlowFactor = 0.65;
constexpr std::uint32_t kMinSetSlowTick = 20;
constexpr std::uint32_t kTransitToHeightSafetyTicks = 50;
constexpr std::uint32_t kTransitToEjectorSafetyTicks = 50;
constexpr std::uint32_t kHeightMeasurementSafetyTicks = 20;
constexpr std::uint32_t kEjectorOccupancySafetyTicks = 20;
constexpr std::uint32_t kTransitToEndSafetyTicks = 30;
constexpr std::uint32_t kTimeoutToleranceTicks = 5;
/** Weiche laenger offen (10ms-Takte); 150 = 1,5 s. */
constexpr int kSwitchOpenHoldTicks = 150;
/** Anlage 1: REQ_TO_GIVE erneut senden, wenn Anlage 2 vorher blockiert war (1 s). */
constexpr int kHandoverRetryIntervalTicks = 100;
/** Anlage 2: maximale Zeit von WP_INFO bis ENTRY_BLOCKED (3 s, 10ms-Takte). */
constexpr int kHandoverEntryTimeoutTicks = 300;
/** Anlage 2: maximal gleichzeitig aktive WPs (inkl. ausstehende Netzwerk-Uebergabe). */
constexpr std::size_t kMaxActiveWorkpiecesAnlage2 = 2;
/** Auswerfer-Anlage: Wartezeit auf Metall-Sensor nach EJECTOR_BLOCKED (10ms-Takte). */
constexpr int kMetalSensorWaitTicks = 40;

std::uint32_t scalePathCmToExtraTicks(std::uint32_t referenceTicksFor4Cm) {
    if (referenceTicksFor4Cm == 0) {
        return 0;
    }

    const double ticksPerCm =
        static_cast<double>(referenceTicksFor4Cm) / kWorkpieceDiameterCm;
    return static_cast<std::uint32_t>((ticksPerCm * kWorstCaseExtraPathCm) + 0.5);
}

std::uint32_t calculateSetSlowTick(std::uint32_t entryToHeightTicksNormal) {
    if (entryToHeightTicksNormal == 0) {
        return kMinSetSlowTick;
    }

    const auto threshold = static_cast<std::uint32_t>(
        (static_cast<double>(entryToHeightTicksNormal) * kSetSlowFactor) + 0.5);
    return std::max(kMinSetSlowTick, threshold);
}

void logTransitTimeoutDiagnostic(const char* segment,
                                 int wpId,
                                 std::uint32_t elapsedTicks,
                                 std::uint32_t expectedLimitTicks,
                                 std::uint32_t toleranceTicks,
                                 const char* motorLabel) {
    const std::uint32_t deadline = expectedLimitTicks + toleranceTicks;
    const std::int32_t overdue =
        static_cast<std::int32_t>(elapsedTicks)
        - static_cast<std::int32_t>(deadline);
    DETAIL_LOG( "[WP-Manager] " << segment << "_TIMEOUT: wp=" << wpId
              << " | Ist: " << elapsedTicks << " Takte (" << (elapsedTicks * 10) << " ms)"
              << " | Soll: max " << expectedLimitTicks << " Takte ("
              << (expectedLimitTicks * 10) << " ms) + " << toleranceTicks
              << " Toleranz (= " << deadline << " Takte / " << (deadline * 10) << " ms)"
              << " | Ueberfaellig: " << overdue << " Takte (" << (overdue * 10) << " ms)"
              << " | Motor: " << motorLabel
             );
}

} // namespace

WorkpieceManager::WorkpieceManager(Logik* context)
    : m_context(context),
      m_speedFactor(1.0),
      m_myPosition(AnlagenPosition::VORN_ANLAGE_1),
      m_isAnlage2Ready(true),
      m_awaitingHandoverReady(false),
      m_handoverInfoSent(false),
      m_handoverMotorActive(false),
      m_handoverReceiveActive(false),
      m_hasPendingNetworkWp(false),
      m_pendingNetworkWpId(-1),
      m_pendingNetworkWpType(WpType::UNKNOWN),
      m_handoverRetryTicks(0),
      m_handoverReceiveTicks(0),
      m_handoverPausedByPartner(false),
      m_isSwitchOpen(false),
      m_switchOpenTicks(0),
      m_isEjectorPushed(false),
      m_ejectorPushTicks(0),
      m_metalWaitWpId(-1),
      m_metalWaitTicks(0),
      m_pendingMetalDecisionWpId(-1),
      m_ejectorLsBlocked(false),
      m_ejectorBlockedWpId(-1),
      m_pendingEjectorBlockedAfterHeight(false),
      TICKS_ONE_SECOND(100)
{}

void WorkpieceManager::setCalibrationData(const CalibrationData& data) {
    m_calibData = data;

}

bool WorkpieceManager::hasWorkpiecesNeedingMotor() const {
    for (const auto& wp : m_activeWorkpieces) {
        switch (wp.getLocation()) {
        case WpLocation::AT_ENTRY:
        case WpLocation::TRANSIT_TO_HEIGHT:
        case WpLocation::AT_HEIGHT_SENSOR:
        case WpLocation::TRANSIT_TO_EJECTOR:
        case WpLocation::AT_EJECTOR:
        case WpLocation::TRANSIT_TO_CHUTE:
        case WpLocation::TRANSIT_TO_END:
            return true;
        default:
            break;
        }
    }
    return false;
}

bool WorkpieceManager::isEntryQueuedBehindLeadingWorkpiece() const {
    const Workpiece* entryWp = nullptr;
    for (const auto& wp : m_activeWorkpieces) {
        if (wp.getLocation() == WpLocation::AT_ENTRY) {
            entryWp = &wp;
            break;
        }
    }
    if (entryWp == nullptr) {
        return false;
    }

    for (const auto& wp : m_activeWorkpieces) {
        if (wp.getId() == entryWp->getId()) {
            continue;
        }
        switch (wp.getLocation()) {
        case WpLocation::TRANSIT_TO_HEIGHT:
        case WpLocation::AT_HEIGHT_SENSOR:
        case WpLocation::TRANSIT_TO_EJECTOR:
        case WpLocation::AT_EJECTOR:
        case WpLocation::TRANSIT_TO_END:
        case WpLocation::TRANSIT_TO_CHUTE:
        case WpLocation::AT_END:
            return true;
        default:
            break;
        }
    }
    return false;
}

bool WorkpieceManager::hasLeadingWorkpieceBeforeHeightSensor() const {
    for (const auto& wp : m_activeWorkpieces) {
        switch (wp.getLocation()) {
        case WpLocation::AT_ENTRY:
        case WpLocation::TRANSIT_TO_HEIGHT:
            return true;
        default:
            break;
        }
    }
    return false;
}

bool WorkpieceManager::isExpectedNetworkEntryAtAnlage2() const {
    return m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2
        && m_hasPendingNetworkWp
        && getFirstWorkpieceAt(WpLocation::AT_ENTRY) == nullptr;
}

bool WorkpieceManager::isEarlyEntryViolationOnBlocked() const {
    const Workpiece* atEntry = getFirstWorkpieceAt(WpLocation::AT_ENTRY);
    const Workpiece* inTransit = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_HEIGHT);

    if (atEntry != nullptr && inTransit != nullptr) {
        return true;
    }

    if (atEntry != nullptr && atEntry->hasEntryBlockedSeen()) {
        return true;
    }

    return false;
}

void WorkpieceManager::addWorkpiece() {
    m_activeWorkpieces.emplace_back();
    Workpiece& newWp = m_activeWorkpieces.back();

    newWp.markEntryBlockedSeen();
    newWp.setTargetLimit(m_calibData.ticks_EntryC_to_HeightB_Normal);
    DETAIL_LOG( "[WP-Manager] CREATE WP lokal: id=" << newWp.getId()
              << " trigger=ENTRY_BLOCKED zone=AT_ENTRY targetLimit="
              << m_calibData.ticks_EntryC_to_HeightB_Normal);
}

void WorkpieceManager::addNetworkWorkpiece(int id, WpType type) {
    m_activeWorkpieces.emplace_back();
    Workpiece& netWp = m_activeWorkpieces.back();

    netWp.setId(id);
    netWp.setPartnerType(type);
    netWp.setType(type);

    netWp.updateLocation(WpLocation::AT_ENTRY);
    netWp.setTargetLimit(m_calibData.ticks_EntryC_to_HeightB_Normal);

    DETAIL_LOG( "[WP-Manager] Netzwerk-Teil von Anlage 1 uebernommen. ID: " << netWp.getId()
              << ", Partner-Typ-ID: " << static_cast<int>(netWp.getPartnerType())
              << " -> Zone: At_Entry");
}

void WorkpieceManager::storePendingNetworkWorkpiece(int id, WpType type) {
    if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2
        && !canAcceptMoreWorkpiecesOnAnlage2()) {
        DETAIL_LOG( "[WP-Manager] Anlage 2: WP-Info abgelehnt "
                  << "(max " << kMaxActiveWorkpiecesAnlage2 << " WPs aktiv).");
        if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
            m_context->getDispatcher()->sendHandoverWpLost();
        }
        return;
    }
    m_pendingNetworkWpId = id;
    m_pendingNetworkWpType = type;
    m_hasPendingNetworkWp = true;
    m_handoverReceiveActive = true;
    m_handoverReceiveTicks = 0;
    DETAIL_LOG( "[WP-Manager] Anlage 2: WP-Info zwischengespeichert id=" << id
              << " type=" << static_cast<int>(type)
              << " (wartet auf ENTRY_BLOCKED).");
}

void WorkpieceManager::commitPendingNetworkWorkpieceAtEntry() {
    if (!m_hasPendingNetworkWp || m_pendingNetworkWpId < 0) {
        return;
    }
    addNetworkWorkpiece(m_pendingNetworkWpId, m_pendingNetworkWpType);
    m_hasPendingNetworkWp = false;
    m_pendingNetworkWpId = -1;
    m_pendingNetworkWpType = WpType::UNKNOWN;
    m_handoverReceiveActive = false;
    m_handoverReceiveTicks = 0;

    if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2
        && m_context != nullptr && m_context->getDispatcher() != nullptr) {
        DETAIL_LOG( "[WP-Manager] Anlage 2: Uebergabe erfolgreich -> melde Anlage 1."
                 );
        m_context->getDispatcher()->sendHandoverSuccessful();
    }
}

void WorkpieceManager::resetHandoverState() {
    m_awaitingHandoverReady = false;
    m_handoverInfoSent = false;
    m_handoverMotorActive = false;
    m_handoverReceiveActive = false;
    m_hasPendingNetworkWp = false;
    m_pendingNetworkWpId = -1;
    m_pendingNetworkWpType = WpType::UNKNOWN;
    m_handoverRetryTicks = 0;
    m_handoverReceiveTicks = 0;
    m_handoverPausedByPartner = false;
}

void WorkpieceManager::recoverPartnerHandoverAfterError() {
    notifyPartnerHandoverSlotFree();
}

bool WorkpieceManager::canAcceptMoreWorkpiecesOnAnlage2() const {
    if (m_myPosition != AnlagenPosition::HINTEN_ANLAGE_2) {
        return true;
    }
    std::size_t occupied = m_activeWorkpieces.size();
    if (m_hasPendingNetworkWp) {
        occupied++;
    }
    return occupied < kMaxActiveWorkpiecesAnlage2;
}

bool WorkpieceManager::canAcceptNetworkHandover() {
    if (m_myPosition != AnlagenPosition::HINTEN_ANLAGE_2) {
        return false;
    }
    if (!canAcceptMoreWorkpiecesOnAnlage2()) {
        return false;
    }
    if (!m_isAnlage2Ready) {
        return false;
    }
    if (m_hasPendingNetworkWp || getFirstWorkpieceAt(WpLocation::AT_ENTRY) != nullptr) {
        return false;
    }
    if (getFirstWorkpieceAt(WpLocation::TRANSIT_TO_HEIGHT) != nullptr) {
        return false;
    }
    if (getFirstWorkpieceAt(WpLocation::TRANSIT_TO_END) != nullptr
        || getFirstWorkpieceAt(WpLocation::AT_END) != nullptr) {
        return false;
    }
    if (m_context == nullptr || m_context->getEventManager() == nullptr) {
        return false;
    }
    const EventManager* em = m_context->getEventManager();
    if (em->isErrorActive() || em->isServiceMode() || em->isConnectionLost()) {
        return false;
    }
    if (em->context().endBlocked) {
        return false;
    }
    return true;
}

void WorkpieceManager::onPartnerHandoverRequest() {
    if (m_myPosition != AnlagenPosition::HINTEN_ANLAGE_2) {
        return;
    }
    if (!canAcceptMoreWorkpiecesOnAnlage2()) {
        DETAIL_LOG( "[WP-Manager] Anlage 2: Uebergabe-Anfrage abgelehnt (max "
                  << kMaxActiveWorkpiecesAnlage2 << " WPs aktiv).");
        return;
    }
    if (!canAcceptNetworkHandover()) {
        DETAIL_LOG( "[WP-Manager] Anlage 2: Uebergabe-Anfrage abgelehnt (nicht bereit)."
                 );
        return;
    }
    if (m_context == nullptr || m_context->getDispatcher() == nullptr) {
        return;
    }
    DETAIL_LOG( "[WP-Manager] Anlage 2: Bereit fuer WP-Uebergabe -> READY_TO_RECEIVE."
             );
    m_context->getDispatcher()->readyToReceiveWp();
}

void WorkpieceManager::onPartnerReadyToReceive() {
    if (m_myPosition != AnlagenPosition::VORN_ANLAGE_1) {
        return;
    }
    Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_END);
    if (m_handoverPausedByPartner && wp != nullptr && m_handoverInfoSent && !m_handoverMotorActive) {
        DETAIL_LOG( "[WP-Manager] Anlage 1: Partner wieder bereit -> "
                  << "pausierte Uebergabe fortsetzen.");
        m_context->processEvent(logik::Event::MOTOR_RUN);
        m_handoverMotorActive = true;
        m_handoverPausedByPartner = false;
        return;
    }
    if (!m_awaitingHandoverReady) {
        if (wp == nullptr || m_handoverInfoSent || m_handoverMotorActive) {
            DETAIL_LOG( "[WP-Manager] Anlage 1: READY_TO_RECEIVE ohne offene Anfrage ignoriert."
                     );
            return;
        }
        m_awaitingHandoverReady = true;
        DETAIL_LOG( "[WP-Manager] Anlage 1: Partner meldet freies Bandende -> "
                  << "Uebergabe starten.");
    }
    if (wp == nullptr) {
        DETAIL_LOG( "[WP-Manager] Anlage 1: READY_TO_RECEIVE aber kein WP am Bandende."
                 );
        if (m_context && m_context->getErrorHandler()) {
            m_context->getErrorHandler()->handleEscalation(
                ErrorType::UNEXPECTED_EVENT, logik::Event::READY_TO_RECEIVE_WP, WpLocation::AT_END);
        }
        m_awaitingHandoverReady = false;
        return;
    }
    if (m_context == nullptr || m_context->getDispatcher() == nullptr) {
        return;
    }
    DETAIL_LOG( "[WP-Manager] Anlage 1: Partner bereit -> sende WP-Info id=" << wp->getId()
              << " und starte Band.");
    m_context->getDispatcher()->sendWorkpieceInfo(wp->getId(), wp->getType());
    m_context->processEvent(logik::Event::MOTOR_RUN);
    m_awaitingHandoverReady = false;
    m_handoverInfoSent = true;
    m_handoverMotorActive = true;
    m_handoverPausedByPartner = false;
    m_handoverRetryTicks = 0;
}

void WorkpieceManager::retryPendingHandoverRequest() {
    if (m_myPosition != AnlagenPosition::VORN_ANLAGE_1) {
        return;
    }
    if (!m_awaitingHandoverReady || m_handoverInfoSent || m_handoverMotorActive) {
        m_handoverRetryTicks = 0;
        return;
    }
    if (getFirstWorkpieceAt(WpLocation::AT_END) == nullptr) {
        m_handoverRetryTicks = 0;
        return;
    }
    EventManager* eventManager = m_context ? m_context->getEventManager() : nullptr;
    if (eventManager == nullptr
        || eventManager->isConnectionLost()
        || !eventManager->isPartnerCalibrationReady()
        || eventManager->isErrorActive()) {
        return;
    }
    if (++m_handoverRetryTicks < kHandoverRetryIntervalTicks) {
        return;
    }
    m_handoverRetryTicks = 0;
    if (m_context == nullptr || m_context->getDispatcher() == nullptr) {
        return;
    }
    DETAIL_LOG( "[WP-Manager] Anlage 1: WP wartet am Bandende -> erneute REQ_TO_GIVE."
             );
    m_context->getDispatcher()->RequestToReceiveWp();
}

void WorkpieceManager::notifyPartnerHandoverSlotFree() {
    if (m_myPosition != AnlagenPosition::HINTEN_ANLAGE_2 || !m_isAnlage2Ready) {
        return;
    }
    if (!canAcceptMoreWorkpiecesOnAnlage2()) {
        DETAIL_LOG( "[WP-Manager] Anlage 2: Kein READY_TO_RECEIVE "
                  << "(max " << kMaxActiveWorkpiecesAnlage2 << " WPs aktiv).");
        return;
    }
    if (getFirstWorkpieceAt(WpLocation::AT_ENTRY) != nullptr
        || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_HEIGHT) != nullptr
        || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_END) != nullptr
        || getFirstWorkpieceAt(WpLocation::AT_END) != nullptr) {
        return;
    }
    if (m_context == nullptr || m_context->getEventManager() == nullptr) {
        return;
    }
    const EventManager* em = m_context->getEventManager();
    if (em->isErrorActive() || em->isServiceMode() || em->isConnectionLost()) {
        return;
    }
    if (m_context->getDispatcher() == nullptr) {
        return;
    }
    if (m_handoverReceiveActive || m_hasPendingNetworkWp) {
        m_handoverReceiveTicks = 0;
    }
    DETAIL_LOG( "[WP-Manager] Anlage 2: Bandende frei -> signalisiere Anlage 1 (READY)."
             );
    m_context->getDispatcher()->readyToReceiveWp();
}

void WorkpieceManager::onPartnerHandoverComplete() {
    if (m_myPosition != AnlagenPosition::VORN_ANLAGE_1) {
        return;
    }
    if (!m_handoverMotorActive) {
        DETAIL_LOG( "[WP-Manager] Anlage 1: HANDOVER_COMPLETE ohne aktive Uebergabe ignoriert."
                 );
        return;
    }
    DETAIL_LOG( "[WP-Manager] Anlage 1: Partner meldet ENTRY_BLOCKED -> Uebergabe abgeschlossen."
             );
    m_handoverMotorActive = false;
    m_handoverInfoSent = false;
    m_handoverPausedByPartner = false;
    if (m_context != nullptr) {
        if (hasWorkpiecesNeedingMotor()) {
            DETAIL_LOG( "[WP-Manager] Anlage 1: Weitere WPs auf Band -> Motor laeuft weiter."
                     );
            m_context->processEvent(logik::Event::MOTOR_RUN);
            if (shouldUseSlowMotor()) {
                m_context->processEvent(logik::Event::SET_SLOW);
            }
        } else {
            DETAIL_LOG( "[WP-Manager] Anlage 1: Keine weiteren WPs -> Motor aus.");
            m_context->processEvent(logik::Event::MOTOR_STOP);
        }
    }
}

void WorkpieceManager::onPartnerHandoverWpLost() {
    if (m_myPosition != AnlagenPosition::VORN_ANLAGE_1) {
        return;
    }
    if (!m_handoverMotorActive && !m_handoverPausedByPartner) {
        return;
    }
    DETAIL_LOG( "[WP-Manager] Anlage 1: Partner meldet WP verloren bei Uebergabe."
             );
    m_handoverMotorActive = false;
    m_handoverInfoSent = false;
    m_awaitingHandoverReady = false;
    m_handoverPausedByPartner = false;
    if (m_context != nullptr) {
        if (hasWorkpiecesNeedingMotor()) {
            DETAIL_LOG( "[WP-Manager] Anlage 1: Weitere WPs auf Band -> Motor laeuft weiter."
                     );
            m_context->processEvent(logik::Event::MOTOR_RUN);
            if (shouldUseSlowMotor()) {
                m_context->processEvent(logik::Event::SET_SLOW);
            }
        } else {
            m_context->processEvent(logik::Event::MOTOR_STOP);
        }
    }
}

void WorkpieceManager::onPartnerHandoverPause() {
    if (m_myPosition != AnlagenPosition::VORN_ANLAGE_1 || !m_handoverMotorActive) {
        return;
    }
    DETAIL_LOG( "[WP-Manager] Anlage 1: Partner blockiert waehrend Uebergabe "
              << "-> Motor pausieren, WP bleibt am Bandende.");
    m_handoverMotorActive = false;
    m_handoverPausedByPartner = true;
    if (m_context != nullptr) {
        m_context->processEvent(logik::Event::MOTOR_STOP);
    }
}

void WorkpieceManager::notifyPartnerHandoverFailed() {
    if (m_myPosition != AnlagenPosition::HINTEN_ANLAGE_2 || !m_handoverReceiveActive) {
        return;
    }
    if (m_context == nullptr || m_context->getDispatcher() == nullptr) {
        return;
    }
    DETAIL_LOG( "[WP-Manager] Anlage 2: Uebergabe fehlgeschlagen -> melde Anlage 1."
             );
    m_context->getDispatcher()->sendHandoverWpLost();
}

void WorkpieceManager::notifyPartnerSequenceSync(WpType type) {
    if (m_myPosition != AnlagenPosition::HINTEN_ANLAGE_2) {
        return;
    }
    if (m_context == nullptr || m_context->getDispatcher() == nullptr) {
        return;
    }
    const WpType syncType = (type != WpType::UNKNOWN) ? type : m_sequence.getExpectedType();
    m_context->getDispatcher()->updateWpListe(syncType);
    DETAIL_LOG( "[WP-Manager] Anlage 2: Rezept-Sync an Anlage 1 gesendet (Typ "
              << static_cast<int>(syncType) << ").");
}

// ============================================================================
// CENTRAL TIMER TICK (ZENTRALER HERZSCHLAG MIT TRANSIT-WATCHDOGS)
// ============================================================================
void WorkpieceManager::taktActuators() {
    if (m_isSwitchOpen) {
        m_switchOpenTicks--;

        if (m_switchOpenTicks <= 0) {
            if (m_context && m_context->getDispatcher()) {
                m_context->getDispatcher()->closeSwitch();
            }
            m_isSwitchOpen = false;
        }
    }

    if (m_isEjectorPushed) {
        m_ejectorPushTicks--;

        if (m_ejectorPushTicks <= 0) {
            if (m_context && m_context->getDispatcher()) {
                m_context->getDispatcher()->pullEjector();
            }
            m_isEjectorPushed = false;
        }
    }

    if (m_metalWaitWpId >= 0) {
        m_metalWaitTicks--;
        if (m_metalWaitTicks <= 0) {
            DETAIL_LOG( "[WP-Manager] Metall-Sensor: Kein Metall-Signal nach "
                      << kMetalSensorWaitTicks << " Takten an der Sortiereinheit."
                     );
                if (m_pendingMetalDecisionWpId == m_metalWaitWpId) {
                if (Workpiece* wp = findWorkpieceById(m_pendingMetalDecisionWpId)) {
                    applyMetalIfDetected(wp);
                    DETAIL_LOG( "[WP-Manager] Metall-Wartezeit abgelaufen -> "
                              << "Sortierentscheidung fuer WP " << wp->getId()
                              << " ohne Metall-Upgrade.");
                    checkAndSetJunction(wp);
                }
                m_pendingMetalDecisionWpId = -1;
            }
            m_metalWaitWpId = -1;
            m_metalWaitTicks = 0;
        }
    }

    if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2
        && m_handoverReceiveActive
        && m_hasPendingNetworkWp) {
        EventManager* eventManager = m_context ? m_context->getEventManager() : nullptr;
        const bool pauseTimer =
            !m_isAnlage2Ready
            || (eventManager != nullptr
                && (eventManager->isErrorActive()
                    || eventManager->isServiceMode()
                    || eventManager->isConnectionLost()
                    || eventManager->isEndBlocked()));
        if (!pauseTimer && ++m_handoverReceiveTicks > kHandoverEntryTimeoutTicks) {
            DETAIL_LOG( "[WP-Manager] Anlage 2: ENTRY_BLOCKED kam nach WP-Info nicht "
                      << "rechtzeitig (id=" << m_pendingNetworkWpId
                      << ", ticks=" << m_handoverReceiveTicks
                      << ", limit=" << kHandoverEntryTimeoutTicks
                      << ") -> Uebergabe-WP verloren.");
            if (m_context && m_context->getErrorHandler()) {
                m_context->getErrorHandler()->handleEscalation(
                    ErrorType::WP_LOST,
                    logik::Event::ENTRY_BLOCKED,
                    WpLocation::AT_ENTRY,
                    m_pendingNetworkWpId,
                    static_cast<std::uint32_t>(m_handoverReceiveTicks),
                    static_cast<std::uint32_t>(kHandoverEntryTimeoutTicks),
                    0);
            }
        }
    }
}

void WorkpieceManager::takt(bool isMotorSlow) {
    const bool motorOn = m_context != nullptr
        && m_context->getEventManager() != nullptr
        && m_context->getEventManager()->isMotorOn();

    for (auto& wp : m_activeWorkpieces) {

        if (wp.getLocation() == WpLocation::AT_HEIGHT_SENSOR) {
            if (motorOn) {
                wp.incrementTicks();
                if (wp.getTargetLimit() > 0
                    && wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    logTransitTimeoutDiagnostic(
                        "HEIGHT_MEASUREMENT",
                        wp.getId(),
                        wp.getTicks(),
                        wp.getTargetLimit(),
                        kTimeoutToleranceTicks,
                        "Slow");
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::STUCK,
                            logik::Event::HEIGHT_BLOCKED,
                            WpLocation::AT_HEIGHT_SENSOR,
                            wp.getId(),
                            wp.getTicks(),
                            wp.getTargetLimit(),
                            kTimeoutToleranceTicks);
                    }
                }
            }
            continue;
        }

        if (wp.getLocation() == WpLocation::AT_EJECTOR) {
            if (motorOn) {
                wp.incrementTicks();
                if (wp.getTargetLimit() > 0
                    && wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    logTransitTimeoutDiagnostic(
                        "EJECTOR_OCCUPANCY",
                        wp.getId(),
                        wp.getTicks(),
                        wp.getTargetLimit(),
                        kTimeoutToleranceTicks,
                        "Slow");
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::STUCK,
                            logik::Event::EJECTOR_BLOCKED,
                            WpLocation::AT_EJECTOR,
                            wp.getId(),
                            wp.getTicks(),
                            wp.getTargetLimit(),
                            kTimeoutToleranceTicks);
                    }
                }
            }
            continue;
        }

        if (wp.getLocation() == WpLocation::AT_CHUTE ||
            wp.getLocation() == WpLocation::AT_END) {
            continue;
        }

        if (!motorOn) {
            continue;
        }

        if (wp.getLocation() == WpLocation::AT_ENTRY
            && isEntryQueuedBehindLeadingWorkpiece()) {
            wp.resetTicks();
            continue;
        }

        wp.incrementTicks();
        updateTargetLimitForMotor(wp, isMotorSlow);

        switch (wp.getLocation()) {
            case WpLocation::AT_ENTRY:
                if (wp.hasEntryBlockedSeen()
                    && wp.getTargetLimit() > 0
                    && wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    DETAIL_LOG( "[WP-Manager] ENTRY_TIMEOUT: wp=" << wp.getId()
                              << " ticks=" << wp.getTicks()
                              << " limit=" << wp.getTargetLimit()
                              << " tolerance=" << kTimeoutToleranceTicks << " -> WP_LOST");
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::WP_LOST, logik::Event::ENTRY_BLOCKED,
                            WpLocation::AT_ENTRY, wp.getId(),
                            wp.getTicks(), wp.getTargetLimit(), kTimeoutToleranceTicks);
                    }
                }
                break;

            case WpLocation::TRANSIT_TO_HEIGHT:
                {
                const std::uint32_t setSlowTick =
                    calculateSetSlowTick(m_calibData.ticks_EntryC_to_HeightB_Normal);

                if (wp.getTicks() == setSlowTick) {
                    const std::uint32_t targetLimit = calculateTransitToHeightLimit(true);
                    DETAIL_LOG( "[WP-Manager] SEND SET_SLOW: wp=" << wp.getId()
                              << " location=TRANSIT_TO_HEIGHT ticks=" << wp.getTicks()
                              << " threshold=" << setSlowTick
                              << " factor=" << kSetSlowFactor
                              << " normalCalib=" << m_calibData.ticks_EntryC_to_HeightB_Normal
                              << " targetLimit=" << targetLimit
                              << " extraNormal="
                              << calculateCalibratedWorstCaseExtraNormal()
                             );
                    m_context->processEvent(logik::Event::SET_SLOW);
                    wp.setTargetLimit(targetLimit);
                }
                }

                // FEHLER: WP verloren vor Höhenmessung -> Zone übergeben
                if (wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    DETAIL_LOG( "[WP-Manager] TRANSIT_TO_HEIGHT_TIMEOUT: wp="
                              << wp.getId()
                              << " ticks=" << wp.getTicks()
                              << " targetLimit=" << wp.getTargetLimit()
                              << " tolerance=" << kTimeoutToleranceTicks << " -> WP_LOST");
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::WP_LOST, logik::Event::UNKNOWN, wp.getLocation(), wp.getId(),
                            wp.getTicks(), wp.getTargetLimit(), kTimeoutToleranceTicks);
                    }
                }
                break;

            case WpLocation::TRANSIT_TO_EJECTOR:
                if (wp.getTargetLimit() > 0
                    && wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    DETAIL_LOG( "[WP-Manager] TRANSIT_TO_EJECTOR_TIMEOUT: wp="
                              << wp.getId()
                              << " ticks=" << wp.getTicks()
                              << " limit=" << wp.getTargetLimit()
                              << " tolerance=" << kTimeoutToleranceTicks << " -> WP_LOST");
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::WP_LOST, logik::Event::UNKNOWN, wp.getLocation(), wp.getId(),
                            wp.getTicks(), wp.getTargetLimit(), kTimeoutToleranceTicks);
                    }
                }
                break;

            case WpLocation::TRANSIT_TO_CHUTE:
                if (wp.getTargetLimit() > 0
                    && wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    handleChuteTransitFailure(wp);
                }
                break;

            case WpLocation::TRANSIT_TO_END:
                if (wp.getTargetLimit() > 0
                    && wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    logTransitTimeoutDiagnostic(
                        "TRANSIT_TO_END",
                        wp.getId(),
                        wp.getTicks(),
                        wp.getTargetLimit(),
                        kTimeoutToleranceTicks,
                        isMotorSlow ? "Slow" : "Fast");
                    syncSequenceAfterLostTransitToEnd(wp);

                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::WP_LOST,
                            logik::Event::UNKNOWN,
                            wp.getLocation(),
                            wp.getId(),
                            wp.getTicks(),
                            wp.getTargetLimit(),
                            kTimeoutToleranceTicks);
                    }
                }
                break;

            default:
                break;
        }
    }

    retryPendingHandoverRequest();
}

// ============================================================================
// SENSOR-SIGNAL VERARBEITUNG
// ============================================================================
void WorkpieceManager::handleSensorSignal(logik::Event e, WpType typeFromHal) {

    switch (e) {
        // --- 1. ZONE: EINLAUF ---
        case logik::Event::ENTRY_BLOCKED: {
            bool acceptedPendingNetworkWorkpiece = false;
            if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2
                && getFirstWorkpieceAt(WpLocation::AT_ENTRY) == nullptr
                && m_hasPendingNetworkWp) {
                commitPendingNetworkWorkpieceAtEntry();
                acceptedPendingNetworkWorkpiece = true;
            }
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_ENTRY);
            if (wp != nullptr) {
                if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
                    auto* eventManager = m_context ? m_context->getEventManager() : nullptr;
                    if (!acceptedPendingNetworkWorkpiece
                        && (eventManager == nullptr || !eventManager->isMotorOn())) {
                        DETAIL_LOG( "[WP-Manager] Anlage 2: ENTRY_BLOCKED vor "
                                  << "Netzwerk-Uebergabe/MOTOR_RUN -> FEHLER.");
                        if (m_context && m_context->getErrorHandler()) {
                            m_context->getErrorHandler()->handleEscalation(
                                ErrorType::UNEXPECTED_EVENT, e, WpLocation::AT_ENTRY, wp->getId());
                        }
                        break;
                    }
                }
                if (!wp->hasEntryBlockedSeen()) {
                    wp->markEntryBlockedSeen();
                    DETAIL_LOG( "[WP-Manager] Netzwerk-WP " << wp->getId()
                              << " erreicht ENTRY_BLOCKED physisch -> Entry-Timer startet."
                             );
                } else {
                    DETAIL_LOG( "[WP-Manager] Ignoriere doppelte ENTRY_BLOCKED-Flanke fuer WP "
                              << wp->getId() << ".");
                }
                if (acceptedPendingNetworkWorkpiece
                    && m_context != nullptr
                    && m_context->getEventManager() != nullptr
                    && !m_context->getEventManager()->isMotorOn()) {
                    DETAIL_LOG( "[WP-Manager] Anlage 2: erwartetes Netzwerk-WP kam bei Motor-Idle "
                              << "-> Motor fuer Einlauf wieder starten.");
                    m_context->processEvent(logik::Event::MOTOR_RUN);
                }
            } else if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
                if (isEarlyEntryViolationOnBlocked()
                    && !isExpectedNetworkEntryAtAnlage2()) {
                    DETAIL_LOG( "[WP-Manager] Anlage 2: ENTRY_BLOCKED bevor vorderes WP "
                              << "den Hoehensensor erreicht hat -> FEHLER.");
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::EARLY_ENTRY, e, WpLocation::AT_ENTRY);
                    }
                } else if (getFirstWorkpieceAt(WpLocation::TRANSIT_TO_HEIGHT) != nullptr
                           && getFirstWorkpieceAt(WpLocation::AT_ENTRY) == nullptr
                           && !m_hasPendingNetworkWp) {
                    DETAIL_LOG( "[WP-Manager] Anlage 2: Ignoriere ENTRY_BLOCKED-Stoerung "
                              << "waehrend WP unterwegs zum HS (kein Teil am Einlauf)."
                             );
                } else if (hasActiveWorkpieces()
                           || m_handoverReceiveActive
                           || m_hasPendingNetworkWp) {
                    DETAIL_LOG( "[WP-Manager] Anlage 2: ENTRY_BLOCKED passt nicht zum "
                              << "aktuellen WP-Zustand -> FEHLER.");
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::UNEXPECTED_EVENT, e, WpLocation::AT_ENTRY);
                    }
                } else {
                    DETAIL_LOG( "[WP-Manager] Anlage 2: ENTRY_BLOCKED ohne WP-Info von "
                              << "Anlage 1 -> FEHLER.");
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::UNEXPECTED_EVENT, e, WpLocation::AT_ENTRY);
                    }
                }
            } else {
                if (getFirstWorkpieceAt(WpLocation::TRANSIT_TO_HEIGHT) != nullptr
                    && getFirstWorkpieceAt(WpLocation::AT_ENTRY) == nullptr) {
                    DETAIL_LOG( "[WP-Manager] Ignoriere ENTRY_BLOCKED-Stoerung "
                              << "waehrend WP unterwegs zum HS (kein Teil am Einlauf)."
                             );
                } else {
                    DETAIL_LOG( "[WP-Manager] ENTRY_BLOCKED ohne aktives WP -> ignoriert.");
                }
            }
            break;
        }

        case logik::Event::ENTRY_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_ENTRY);
            if (wp != nullptr) {
                const std::uint32_t entryBlockedTicks = wp->getTicks();
                const std::uint32_t worstCaseExtraTicks =
                    calculateCalibratedWorstCaseExtraNormal();
                const std::uint32_t transitLimit = calculateTransitToHeightLimit(false);

                DETAIL_LOG( "[HAL-Signal] ENTRY_CLEARED fuer WP " << wp->getId()
                          << " -> Transit zum HS. Erwartet EntryC->HeightB Normal="
                          << m_calibData.ticks_EntryC_to_HeightB_Normal
                          << " Takte, Slow=" << m_calibData.ticks_EntryC_to_HeightB_Slow
                          << " Takte.");
                DETAIL_LOG( "[WP-Manager] ENTRY_CLEARED limit: wp=" << wp->getId()
                          << " entryBlockedTicks=" << entryBlockedTicks
                          << " extraWorstCaseNormal=" << worstCaseExtraTicks
                          << " (kalibriert aus EntryB->EntryC, nicht gemessen)"
                          << " targetLimit=" << transitLimit
                         );

                wp->updateLocation(WpLocation::TRANSIT_TO_HEIGHT);
                wp->setTargetLimit(transitLimit);
            } else if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2
                       && hasActiveWorkpieces()) {
                DETAIL_LOG( "[WP-Manager] Anlage 2: ENTRY_CLEARED ohne WP am Einlauf "
                          << "-> passt nicht zum Schieberegister -> FEHLER.");
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(
                        ErrorType::UNEXPECTED_EVENT, e, WpLocation::AT_ENTRY);
                }
            } else {
                DETAIL_LOG( "[WP-Manager] ENTRY_CLEARED ohne WP am Einlauf -> ignoriert "
                          << "(spaete Flanke oder bereits unterwegs).");
            }
            break;
        }

        // --- 2. ZONE: HÖHENMESSUNG & METALLERKENNUNG ---
        case logik::Event::HEIGHT_BLOCKED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_HEIGHT);
            if (wp != nullptr) {
                const auto consumedTicks = wp->getTicks();
                DETAIL_LOG( "[WP-Manager] HEIGHT_BLOCKED: wp=" << wp->getId()
                          << " verbrauchteTakte=" << consumedTicks
                          << " (ca. " << (consumedTicks * 10) << "ms)"
                          << " kalibriertNormal=" << m_calibData.ticks_EntryC_to_HeightB_Normal
                          << " kalibriertSlow=" << m_calibData.ticks_EntryC_to_HeightB_Slow
                          << " targetLimit=" << wp->getTargetLimit()
                         );
                DETAIL_LOG( "[HAL-Signal] WP " << wp->getId()
                          << " an Hoehenmessung eingetroffen.");
                wp->updateLocation(WpLocation::AT_HEIGHT_SENSOR);
                {
                    const std::uint32_t limit = calculateHeightMeasurementLimit();
                    wp->setTargetLimit(limit);
                    DETAIL_LOG( "[WP-Manager] Hoehenmessung-Timeout: wp=" << wp->getId()
                              << " limit=" << limit
                              << " (HeightB->HeightC Slow="
                              << m_calibData.ticks_HeightB_to_HeightC_Slow
                              << ", Normal=" << m_calibData.ticks_HeightB_to_HeightC_Normal
                              << ", speedFactor=" << m_speedFactor << ")"
                             );
                }
                if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
                    DETAIL_LOG( "[WP-Manager] Anlage 2: WP am Hoehensensor -> "
                              << "Mindestabstand fuer naechste Uebergabe erreicht."
                             );
                    notifyPartnerHandoverSlotFree();
                }
            } else if (getFirstWorkpieceAt(WpLocation::AT_HEIGHT_SENSOR) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR) != nullptr) {
                DETAIL_LOG( "[WP-Manager] Ignoriere doppelte/spaete HEIGHT_BLOCKED-Flanke.");
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

         case logik::Event::HEIGHT_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_HEIGHT_SENSOR);
            if (wp == nullptr) {
                wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_HEIGHT);
                if (wp != nullptr) {
                    DETAIL_LOG( "[WP-Manager] HEIGHT_CLEARED ohne vorher verbuchtes "
                              << "HEIGHT_BLOCKED fuer WP " << wp->getId()
                              << " -> Hoehensensor-Flanke logisch nachgezogen."
                             );
                    wp->updateLocation(WpLocation::AT_HEIGHT_SENSOR);
                }
            }
            if (wp != nullptr) {
                wp->setMeasuredType(typeFromHal);
                applyMetalIfDetected(wp);
                DETAIL_LOG( "[HAL-Signal] Messung beendet fuer WP " << wp->getId()
                          << ". Lokal gemessener Typ: " << static_cast<int>(wp->getMeasuredType())
                          << " -> Transit zur Weiche.");

                wp->updateLocation(WpLocation::TRANSIT_TO_EJECTOR);
                {
                    const std::uint32_t limit = calculateEjectorTransitLimit();
                    wp->setTargetLimit(limit);
                    DETAIL_LOG( "[WP-Manager] TRANSIT_TO_EJECTOR limit: wp=" << wp->getId()
                              << " normal=" << m_calibData.ticks_HeightC_to_EjectorB_Normal
                              << " slowCalib=" << m_calibData.ticks_HeightC_to_EjectorB_Slow
                              << " slowScaled="
                              << scaleNormalToSlowTicks(
                                     m_calibData.ticks_HeightC_to_EjectorB_Normal)
                              << " targetLimit=" << limit);
                }
                if (m_pendingEjectorBlockedAfterHeight) {
                    m_pendingEjectorBlockedAfterHeight = false;
                    DETAIL_LOG( "[WP-Manager] Vorgezogene EJECTOR_BLOCKED-Flanke "
                              << "nach HEIGHT_CLEARED fuer WP " << wp->getId()
                              << " verarbeitet.");
                    m_ejectorLsBlocked = true;
                    wp->updateLocation(WpLocation::AT_EJECTOR);
                    m_ejectorBlockedWpId = wp->getId();
                    wp->setTargetLimit(calculateEjectorOccupancyLimit());
                    startSorterDecisionForWp(wp);
                }
            } else if (getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR) != nullptr
                       || getFirstWorkpieceAt(WpLocation::AT_EJECTOR) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_END) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_CHUTE) != nullptr) {
                DETAIL_LOG( "[WP-Manager] Ignoriere doppelte/spaete HEIGHT_CLEARED-Flanke "
                          << "(Typ=" << static_cast<int>(typeFromHal) << ").");
            } else if (m_context != nullptr
                       && m_context->getEventManager() != nullptr
                       && m_context->getEventManager()->isErrorActive()) {
                DETAIL_LOG( "[WP-Manager] HEIGHT_CLEARED im ErrorState ignoriert "
                          << "(Typ=" << static_cast<int>(typeFromHal) << ").");
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

        case logik::Event::HEIGHT_ERROR: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_HEIGHT_SENSOR);
            if (wp == nullptr) {
                wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_HEIGHT);
            }
            const int id = wp ? wp->getId() : -1;
            const WpLocation location =
                wp ? wp->getLocation() : WpLocation::AT_HEIGHT_SENSOR;

            if (m_context && m_context->getErrorHandler()) {
                m_context->getErrorHandler()->handleEscalation(
                    ErrorType::HEIGHT_ERROR, e, location, id);
            }
            break;
        }

        case logik::Event::METAL_DETECTED: {
            Workpiece* wp = getLeadingWorkpieceForMetalDetection();
            if (wp != nullptr) {
                wp->setMetalDetected(true);
                const WpType typeBeforeMetal = wp->getType();
                if (typeBeforeMetal != WpType::UNKNOWN) {
                    applyMetalTypeUpgrade(wp);
                }
                const bool upgradedByMetal =
                    typeBeforeMetal != WpType::UNKNOWN && wp->getType() != typeBeforeMetal;
                if (m_metalWaitWpId == wp->getId()) {
                    m_metalWaitWpId = -1;
                    m_metalWaitTicks = 0;
                    DETAIL_LOG( "[WP-Manager] Metall erkannt waehrend Sortierentscheidung "
                              << "(WP id=" << wp->getId() << ").");
                    if (m_pendingMetalDecisionWpId == wp->getId()) {
                        DETAIL_LOG( "[WP-Manager] Metall erkannt -> "
                                  << "nachgezogene Sortierentscheidung fuer WP "
                                  << wp->getId() << ".");
                        m_pendingMetalDecisionWpId = -1;
                        checkAndSetJunction(wp);
                    }
                } else if (typeBeforeMetal == WpType::UNKNOWN) {
                    DETAIL_LOG( "[WP-Manager] Metall erkannt vor HS-Ende "
                              << "(WP id=" << wp->getId()
                              << ", zone=" << static_cast<int>(wp->getLocation())
                              << ") -> Flag gespeichert.");
                } else if (upgradedByMetal) {
                    DETAIL_LOG( "[WP-Manager] Metall erkannt waehrend Transit "
                              << "(WP id=" << wp->getId() << ").");
                } else {
                    DETAIL_LOG( "[WP-Manager] Metall erkannt (WP id=" << wp->getId()
                              << ", Typ unveraendert).");
                }
            } else if (getFirstWorkpieceAt(WpLocation::TRANSIT_TO_END) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_CHUTE) != nullptr
                       || getFirstWorkpieceAt(WpLocation::AT_CHUTE) != nullptr) {
                DETAIL_LOG( "[WP-Manager] Ignoriere spaetes METAL_DETECTED.");
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

        case logik::Event::METAL_CLEARED: {
            break;
        }

        // --- 3. ZONE: SORTIERWEICHE (EJECTOR) ---
        case logik::Event::EJECTOR_BLOCKED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR);
            if (wp != nullptr) {
                m_ejectorLsBlocked = true;
                m_ejectorBlockedWpId = wp->getId();
                DETAIL_LOG( "[HAL-Signal] WP " << wp->getId() << " erreicht physisch die Sortierweiche.");
                wp->updateLocation(WpLocation::AT_EJECTOR);
                wp->setTargetLimit(calculateEjectorOccupancyLimit());
                startSorterDecisionForWp(wp);
            } else if (getFirstWorkpieceAt(WpLocation::AT_HEIGHT_SENSOR) != nullptr) {
                m_pendingEjectorBlockedAfterHeight = true;
                DETAIL_LOG( "[WP-Manager] EJECTOR_BLOCKED kam vor HEIGHT_CLEARED -> "
                          << "wird fuer aktuelles Hoehenmessungs-WP gepuffert.");
            } else if (getFirstWorkpieceAt(WpLocation::AT_EJECTOR) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_END) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_CHUTE) != nullptr) {
                DETAIL_LOG( "[WP-Manager] Ignoriere doppelte/spaete EJECTOR_BLOCKED-Flanke.");
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

        case logik::Event::EJECTOR_CLEARED: {
            const bool wasEjectorBlocked = m_ejectorLsBlocked;
            m_ejectorLsBlocked = false;

            auto processEjectorClearedForWp = [this](Workpiece* wp) {
                if (wp == nullptr) {
                    return;
                }
                if (m_ejectorBlockedWpId >= 0 && wp->getId() != m_ejectorBlockedWpId) {
                    DETAIL_LOG( "[WP-Manager] Ignoriere EJECTOR_CLEARED fuer WP "
                              << wp->getId()
                              << " (Sortier-LS gehoert zu WP " << m_ejectorBlockedWpId
                              << ").");
                    return;
                }
                if (m_pendingMetalDecisionWpId == wp->getId()) {
                    DETAIL_LOG( "[WP-Manager] EJECTOR_CLEARED waehrend Metall-Wartezeit "
                              << "(WP id=" << wp->getId()
                              << ") -> Sortierentscheidung jetzt ohne Metall-Upgrade."
                             );
                    m_pendingMetalDecisionWpId = -1;
                    m_metalWaitWpId = -1;
                    m_metalWaitTicks = 0;
                    applyMetalIfDetected(wp);
                    checkAndSetJunction(wp);
                }
                if (wp->isCorrectInSequence() || wp->isDeferRejectToPartner()) {
                    wp->updateLocation(WpLocation::TRANSIT_TO_END);
                    const bool isMotorSlow =
                        m_context != nullptr
                        && m_context->getEventManager() != nullptr
                        && m_context->getEventManager()->isMotorSlow();
                    const std::uint32_t endLimit = calculateEndTransitLimit(isMotorSlow);
                    wp->setTargetLimit(endLimit);
                    DETAIL_LOG( "[WP-Manager] EJECTOR_CLEARED: WP " << wp->getId()
                              << " -> Transit zum Bandende. Soll-Ankunft END max "
                              << endLimit << " Takte (" << (endLimit * 10) << " ms)"
                              << " Motor=" << (isMotorSlow ? "Slow" : "Fast")
                              << " calibNormal=" << m_calibData.ticks_EjectorC_to_EndB_Normal
                             );
                    if (wp->isCorrectInSequence()) {
                        advanceTargetSequence();
                    }
                    if (wp->isDeferRejectToPartner()) {
                        wp->setDeferRejectToPartner(false);
                    }
                } else {
                    wp->updateLocation(WpLocation::TRANSIT_TO_CHUTE);
                    const std::uint32_t chuteSlow = m_calibData.ticks_EjectorC_to_ChuteB_Slow;
                    const std::uint32_t chuteLimit = chuteSlow > 0
                        ? std::max(chuteSlow, 150U)
                        : 150U;
                    wp->setTargetLimit(chuteLimit);
                    DETAIL_LOG( "[WP-Manager] EJECTOR_CLEARED: WP " << wp->getId()
                              << " -> Transit zur Rutsche.");
                }
                m_ejectorBlockedWpId = -1;
            };

            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_EJECTOR);
            if (wp != nullptr) {
                processEjectorClearedForWp(wp);
                promoteTrailingWorkpieceAtSorterIfDue("EJECTOR_CLEARED");
            } else if (promoteTrailingWorkpieceAtSorterIfDue("EJECTOR_CLEARED")) {
                processEjectorClearedForWp(getFirstWorkpieceAt(WpLocation::AT_EJECTOR));
            } else if (Workpiece* transitWp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR)) {
                const std::uint32_t limit =
                    transitWp->getTargetLimit() > 0
                        ? transitWp->getTargetLimit()
                        : calculateEjectorTransitLimit();
                const std::uint32_t threshold =
                    limit > kTransitToEjectorSafetyTicks
                        ? limit - kTransitToEjectorSafetyTicks
                        : limit;

                // CLEARED-Flanken vom vorherigen WP (Weiche/Auswerfer) duerfen ein
                // nachfolgendes WP in TRANSIT nicht vorzeitig an der Sortier-LS platzieren.
                if (!wasEjectorBlocked && transitWp->getTicks() < threshold) {
                    DETAIL_LOG( "[WP-Manager] Ignoriere EJECTOR_CLEARED fuer WP "
                              << transitWp->getId()
                              << " (noch TRANSIT_TO_EJECTOR, ticks="
                              << transitWp->getTicks()
                              << ", threshold=" << threshold
                              << ") – Flanke gehoert zum vorherigen WP."
                             );
                } else {
                    DETAIL_LOG( "[WP-Manager] EJECTOR_CLEARED ohne AT_EJECTOR, aber WP "
                              << transitWp->getId()
                              << " ist noch TRANSIT_TO_EJECTOR -> Sortierentscheidung "
                              << "fuer diese Flanke nachgezogen.");
                    m_ejectorLsBlocked = true;
                    transitWp->updateLocation(WpLocation::AT_EJECTOR);
                    transitWp->setTargetLimit(calculateEjectorOccupancyLimit());
                    m_ejectorBlockedWpId = transitWp->getId();
                    applyMetalIfDetected(transitWp);
                    const bool needsMetalWait = shouldDeferJunctionForMetal(transitWp);
                    startSorterDecisionForWp(transitWp);
                    if (!needsMetalWait) {
                        processEjectorClearedForWp(transitWp);
                    }
                }
            } else if (getFirstWorkpieceAt(WpLocation::TRANSIT_TO_END) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_CHUTE) != nullptr
                       || getFirstWorkpieceAt(WpLocation::AT_CHUTE) != nullptr
                       || getFirstWorkpieceAt(WpLocation::AT_END) != nullptr
                       || getFirstWorkpieceAt(WpLocation::AT_HEIGHT_SENSOR) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_HEIGHT) != nullptr
                       || getFirstWorkpieceAt(WpLocation::AT_ENTRY) != nullptr) {
                DETAIL_LOG( "[WP-Manager] Ignoriere doppelte/spaete EJECTOR_CLEARED-Flanke.");
            } else if (!wasEjectorBlocked && !m_activeWorkpieces.empty()) {
                DETAIL_LOG( "[WP-Manager] Ignoriere EJECTOR_CLEARED waehrend Mehrfach-Betrieb "
                          << "(LS bereits frei, WP bereits verlassen).");
            } else if (wasEjectorBlocked && !m_activeWorkpieces.empty()) {
                DETAIL_LOG( "[WP-Manager] Ignoriere spaetes EJECTOR_CLEARED "
                          << "(WP physisch bereits aus Weichenzone).");
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

        // --- 4. ZONE: BANDENDE ---
        case logik::Event::END_BLOCKED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_END);
            if (wp != nullptr) {
                wp->updateLocation(WpLocation::AT_END);

                if (m_myPosition == AnlagenPosition::VORN_ANLAGE_1) {
                    EventManager* eventManager =
                        m_context ? m_context->getEventManager() : nullptr;
                    const bool partnerOk = eventManager != nullptr
                        && !eventManager->isConnectionLost()
                        && eventManager->isPartnerCalibrationReady();

                    m_awaitingHandoverReady = true;
                    m_handoverRetryTicks = 0;

                    if (!partnerOk) {
                        if (wp->isCorrectInSequence()) {
                            m_sequence.forceType(wp->getType());
                        }
                        DETAIL_LOG( "[WP-Manager] Anlage 1: Bandende belegt, Partner nicht "
                                  << "bereit -> warte ohne Uebergabe-Anfrage.");
                    } else if (m_context && m_context->getDispatcher()) {
                        DETAIL_LOG( "[WP-Manager] Anlage 1: WP id=" << wp->getId()
                                  << " am Bandende -> frage Anlage 2 (REQ_TO_GIVE)."
                                 );
                        m_context->getDispatcher()->RequestToReceiveWp();
                    }
                } else if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
                    setAnlage2Ready(false);
                    if ((m_handoverReceiveActive || m_hasPendingNetworkWp)
                        && m_context != nullptr
                        && m_context->getDispatcher() != nullptr) {
                        DETAIL_LOG( "[WP-Manager] Anlage 2: Bandende wurde waehrend "
                                  << "Uebergabe belegt -> Anlage 1 pausieren."
                                 );
                        m_context->getDispatcher()->sendHandoverPause();
                    }
                    DETAIL_LOG( "[WP-Manager] Anlage 2: WP id=" << wp->getId()
                              << " am Bandende -> Motor stoppt, keine Uebergabe bis END_CLEARED."
                             );
                }
            } else if (getFirstWorkpieceAt(WpLocation::AT_END) != nullptr) {
                DETAIL_LOG( "[WP-Manager] Ignoriere doppelte END_BLOCKED-Flanke.");
            } else if (m_myPosition == AnlagenPosition::VORN_ANLAGE_1
                       && (getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR) != nullptr
                           || getFirstWorkpieceAt(WpLocation::AT_EJECTOR) != nullptr)) {
                Workpiece* lateWp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR);
                if (lateWp == nullptr) {
                    lateWp = getFirstWorkpieceAt(WpLocation::AT_EJECTOR);
                }

                const WpType expectedType = m_sequence.getExpectedType();
                const bool recipeOk = m_sequence.isCorrectType(lateWp->getType());
                const bool metalCompatible =
                    (lateWp->getType() == WpType::TALL && expectedType == WpType::TALL_METAL)
                    || (lateWp->getType() == WpType::TALL_DRILLED
                        && expectedType == WpType::TALL_DRILLED_METAL);
                if (recipeOk || metalCompatible) {
                    if (metalCompatible) {
                        lateWp->setMeasuredType(expectedType);
                        DETAIL_LOG( "[WP-Manager] Anlage 1: END_BLOCKED fuer metall-kompatibles WP id="
                                  << lateWp->getId()
                                  << " -> finaler Typ auf erwarteten Metalltyp gesetzt."
                                 );
                    }
                    DETAIL_LOG( "[WP-Manager] Anlage 1: END_BLOCKED vor Sortier-LS-Abschluss "
                              << "fuer korrektes WP id=" << lateWp->getId()
                              << " -> als Bandende akzeptiert.");
                    lateWp->setIsCorrectInSequence(true);
                    lateWp->updateLocation(WpLocation::AT_END);
                    advanceTargetSequence();

                    EventManager* eventManager =
                        m_context ? m_context->getEventManager() : nullptr;
                    const bool partnerOk = eventManager != nullptr
                        && !eventManager->isConnectionLost()
                        && eventManager->isPartnerCalibrationReady();

                    m_awaitingHandoverReady = true;
                    m_handoverRetryTicks = 0;

                    if (!partnerOk) {
                        DETAIL_LOG( "[WP-Manager] Anlage 1: Bandende belegt, Partner nicht "
                                  << "bereit -> warte ohne Uebergabe-Anfrage.");
                    } else if (m_context && m_context->getDispatcher()) {
                        DETAIL_LOG( "[WP-Manager] Anlage 1: WP id=" << lateWp->getId()
                                  << " am Bandende -> frage Anlage 2 (REQ_TO_GIVE)."
                                 );
                        m_context->getDispatcher()->RequestToReceiveWp();
                    }
                } else {
                    DETAIL_LOG( "[WP-Manager] Anlage 1: END_BLOCKED fuer nicht freigegebenes WP id="
                              << lateWp->getId()
                              << " (lokal=" << static_cast<int>(lateWp->getType())
                              << ", erwartet=" << static_cast<int>(m_sequence.getExpectedType())
                              << ") -> Fehler.");
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::UNEXPECTED_EVENT, e, lateWp->getLocation(), lateWp->getId());
                    }
                }
            } else if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2
                       && (getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR) != nullptr
                           || getFirstWorkpieceAt(WpLocation::AT_EJECTOR) != nullptr)) {
                Workpiece* lateWp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR);
                if (lateWp == nullptr) {
                    lateWp = getFirstWorkpieceAt(WpLocation::AT_EJECTOR);
                }

                const WpType partnerType = lateWp->getPartnerType();
                const WpType localType = lateWp->getType();
                const WpType expectedType = m_sequence.getExpectedType();
                const bool partnerTypeOk =
                    partnerType == WpType::UNKNOWN || partnerType == localType;
                const bool recipeOk = m_sequence.isCorrectType(localType);
                const bool metalCompatible =
                    ((localType == WpType::TALL && expectedType == WpType::TALL_METAL)
                     || (localType == WpType::TALL_DRILLED
                         && expectedType == WpType::TALL_DRILLED_METAL))
                    && (partnerType == WpType::UNKNOWN || partnerType == expectedType);

                if ((partnerTypeOk && recipeOk) || metalCompatible) {
                    if (metalCompatible) {
                        lateWp->setMeasuredType(expectedType);
                        DETAIL_LOG( "[WP-Manager] Anlage 2: END_BLOCKED fuer metall-kompatibles WP id="
                                  << lateWp->getId()
                                  << " -> finaler Typ auf erwarteten Metalltyp gesetzt."
                                 );
                    }
                    DETAIL_LOG( "[WP-Manager] Anlage 2: END_BLOCKED vor Sortier-LS-Abschluss "
                              << "fuer korrektes WP id=" << lateWp->getId()
                              << " -> als Bandende akzeptiert.");
                    lateWp->setIsCorrectInSequence(true);
                    lateWp->updateLocation(WpLocation::AT_END);
                    advanceTargetSequence();
                    setAnlage2Ready(false);
                    if ((m_handoverReceiveActive || m_hasPendingNetworkWp)
                        && m_context != nullptr
                        && m_context->getDispatcher() != nullptr) {
                        DETAIL_LOG( "[WP-Manager] Anlage 2: Bandende wurde waehrend "
                                  << "Uebergabe belegt -> Anlage 1 pausieren."
                                 );
                        m_context->getDispatcher()->sendHandoverPause();
                    }
                    DETAIL_LOG( "[WP-Manager] Anlage 2: WP id=" << lateWp->getId()
                              << " am Bandende -> Motor stoppt, keine Uebergabe bis END_CLEARED."
                             );
                } else {
                    DETAIL_LOG( "[WP-Manager] Anlage 2: END_BLOCKED fuer nicht freigegebenes WP id="
                              << lateWp->getId()
                              << " (Partner=" << static_cast<int>(lateWp->getPartnerType())
                              << ", lokal=" << static_cast<int>(lateWp->getType())
                              << ", erwartet=" << static_cast<int>(m_sequence.getExpectedType())
                              << ") -> Fehler.");
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::UNEXPECTED_EVENT, e, lateWp->getLocation(), lateWp->getId());
                    }
                }
            } else if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2
                       && hasActiveWorkpieces()) {
                DETAIL_LOG( "[WP-Manager] Anlage 2: END_BLOCKED passt nicht zum "
                          << "aktuellen WP-Zustand -> FEHLER.");
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(
                        ErrorType::UNEXPECTED_EVENT, e);
                }
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

        case logik::Event::END_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_END);
            if (wp != nullptr) {
                if (m_myPosition == AnlagenPosition::VORN_ANLAGE_1) {
                    if (m_awaitingHandoverReady) {
                        DETAIL_LOG( "[WP-Manager] Anlage 1: END_CLEARED vor Partner-Bestaetigung."
                                 );
                        if (m_context && m_context->getErrorHandler()) {
                            m_context->getErrorHandler()->handleEscalation(
                                ErrorType::UNEXPECTED_EVENT, e, WpLocation::AT_END, wp->getId());
                        }
                        m_awaitingHandoverReady = false;
                        break;
                    }
                    if (!m_handoverInfoSent) {
                        if (wp->isCorrectInSequence()) {
                            m_sequence.forceType(wp->getType());
                        }
                        if (m_context && m_context->getErrorHandler()) {
                            m_context->getErrorHandler()->handleEscalation(
                                ErrorType::UNEXPECTED_EVENT, e, WpLocation::AT_END, wp->getId());
                        }
                        break;
                    }
                    m_handoverInfoSent = false;
                } else if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
                    setAnlage2Ready(true);
                    DETAIL_LOG( "[WP-Manager] Anlage 2: Bandende frei -> wieder bereit fuer Uebergabe."
                             );
                }
                if (wp->isCorrectInSequence()) {
                    operatorLog::logWorkpieceBandExit(
                        wp->getId(), wp->getType(), "Zielband");
                }
                removeWorkpiece(wp->getId());
                if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
                    notifyPartnerHandoverSlotFree();
                }
            } else if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2
                       && hasActiveWorkpieces()) {
                DETAIL_LOG( "[WP-Manager] Anlage 2: END_CLEARED ohne WP am Bandende "
                          << "-> passt nicht zum Schieberegister -> FEHLER.");
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(
                        ErrorType::UNEXPECTED_EVENT, e, WpLocation::AT_END);
                }
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

        // --- 5. ZONE: SORTIERRUTSCHE (CHUTE) ---
        case logik::Event::CHUTE_BLOCKED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_CHUTE);
            if (wp != nullptr) {
                DETAIL_LOG( "[WP-Manager] CHUTE_BLOCKED: WP " << wp->getId()
                          << " in Rutsche angekommen -> aus aktiver Liste entfernen."
                         );
                operatorLog::logWorkpieceBandExit(
                    wp->getId(), wp->getType(), "Aussortierung");
                removeWorkpiece(wp->getId());
                promoteTrailingWorkpieceAtSorterIfDue("CHUTE_BLOCKED");
            } else {
                DETAIL_LOG( "[WP-Manager] CHUTE_BLOCKED ohne WP ignoriert (nur Fuellstand/Warnung).");
            }
            break;
        }

        case logik::Event::CHUTE_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_CHUTE);
            if (wp != nullptr) {
                removeWorkpiece(wp->getId());
            } else {
                DETAIL_LOG( "[WP-Manager] CHUTE_CLEARED ohne WP ignoriert (nur Fuellstand/Warnung).");
            }
            break;
        }

        default:
            break;
    }
}

// ============================================================================
// HILFSMETHODEN
// ============================================================================
const Workpiece* WorkpieceManager::getFirstWorkpieceAt(WpLocation loc) const {
    for (const auto& wp : m_activeWorkpieces) {
        if (wp.getLocation() == loc) {
            return &wp;
        }
    }
    return nullptr;
}

Workpiece* WorkpieceManager::getFirstWorkpieceAt(WpLocation loc) {
    for (auto& wp : m_activeWorkpieces) {
        if (wp.getLocation() == loc) {
            return &wp;
        }
    }
    return nullptr;
}

void WorkpieceManager::removeWorkpiece(int id) {
    m_activeWorkpieces.erase(
        std::remove_if(m_activeWorkpieces.begin(), m_activeWorkpieces.end(),
                       [id](const Workpiece& wp) { return wp.getId() == id; }),
        m_activeWorkpieces.end()
    );

    if (m_activeWorkpieces.empty() && m_context != nullptr) {
        m_context->processEvent(logik::Event::BAND_EMPTY);
    }
}

std::uint32_t WorkpieceManager::scaleNormalToSlowTicks(std::uint32_t normalTicks) const {
    if (normalTicks == 0) {
        return 0;
    }
    if (m_speedFactor <= 1.0) {
        return normalTicks;
    }
    return static_cast<std::uint32_t>(static_cast<double>(normalTicks) * m_speedFactor + 0.5);
}

std::uint32_t WorkpieceManager::calculateCalibratedWorstCaseExtraNormal() const {
    return scalePathCmToExtraTicks(m_calibData.ticks_EntryB_to_EntryC_Normal);
}

std::uint32_t WorkpieceManager::calculateTransitToHeightLimit(bool afterSetSlow) const {
    const std::uint32_t normal = m_calibData.ticks_EntryC_to_HeightB_Normal;
    const std::uint32_t slowFull = m_calibData.ticks_EntryC_to_HeightB_Slow > 0
        ? m_calibData.ticks_EntryC_to_HeightB_Slow
        : scaleNormalToSlowTicks(normal);
    const std::uint32_t worstExtraNormal = calculateCalibratedWorstCaseExtraNormal();
    const std::uint32_t worstExtraSlow = scaleNormalToSlowTicks(worstExtraNormal);

    if (normal == 0) {
        return slowFull + worstExtraSlow + kTransitToHeightSafetyTicks;
    }

    if (!afterSetSlow) {
        return normal + worstExtraNormal + kTransitToHeightSafetyTicks;
    }

    const std::uint32_t setSlowTick = calculateSetSlowTick(normal);
    const double restAnteilNormal =
        std::max(0.0, 1.0 - (static_cast<double>(setSlowTick)
            / static_cast<double>(normal)));
    const std::uint32_t restTakteSlow =
        static_cast<std::uint32_t>(restAnteilNormal * static_cast<double>(slowFull));
    const std::uint32_t mixedSpeedTarget =
        setSlowTick + restTakteSlow + worstExtraSlow + kTransitToHeightSafetyTicks;
    const std::uint32_t conservativeSlowTarget =
        slowFull + worstExtraSlow + kTransitToHeightSafetyTicks;
    return std::max(mixedSpeedTarget, conservativeSlowTarget);
}

std::uint32_t WorkpieceManager::calculateHeightMeasurementLimit() const {
    const std::uint32_t normal = m_calibData.ticks_HeightB_to_HeightC_Normal;
    const std::uint32_t slowCalib = m_calibData.ticks_HeightB_to_HeightC_Slow;
    const std::uint32_t slowScaled = normal > 0 ? scaleNormalToSlowTicks(normal) : 0U;
    const std::uint32_t slowFull = std::max(slowCalib, slowScaled);
    const std::uint32_t base = std::max({normal, slowFull});
    if (base == 0) {
        return TICKS_ONE_SECOND;
    }
    return base + kHeightMeasurementSafetyTicks;
}

std::uint32_t WorkpieceManager::calculateEjectorOccupancyLimit() const {
    const std::uint32_t normal = m_calibData.ticks_EjectorB_to_EjectorC_Normal;
    const std::uint32_t slowCalib = m_calibData.ticks_EjectorB_to_EjectorC_Slow;
    const std::uint32_t slowScaled = normal > 0 ? scaleNormalToSlowTicks(normal) : 0U;
    const std::uint32_t slowFull = std::max(slowCalib, slowScaled);
    const std::uint32_t base = std::max({normal, slowFull});
    if (base == 0) {
        return TICKS_ONE_SECOND;
    }
    return base + kMetalSensorWaitTicks + kEjectorOccupancySafetyTicks;
}

std::uint32_t WorkpieceManager::calculateEjectorTransitLimit() const {
    const std::uint32_t normal = m_calibData.ticks_HeightC_to_EjectorB_Normal;
    const std::uint32_t slowCalib = m_calibData.ticks_HeightC_to_EjectorB_Slow;
    const std::uint32_t slowScaled = normal > 0 ? scaleNormalToSlowTicks(normal) : 0U;
    const std::uint32_t slowFull = std::max(slowCalib, slowScaled);
    const std::uint32_t base = std::max(normal, slowFull);
    if (base == 0) {
        return 0;
    }
    return base + kTransitToEjectorSafetyTicks;
}

std::uint32_t WorkpieceManager::calculateEndTransitLimit(bool isMotorSlow) const {
    const std::uint32_t normal = m_calibData.ticks_EjectorC_to_EndB_Normal;
    if (normal == 0) {
        return TICKS_ONE_SECOND;
    }
    const std::uint32_t base = isMotorSlow
        ? std::max(normal, scaleNormalToSlowTicks(normal))
        : normal;
    return base + kTransitToEndSafetyTicks;
}

void WorkpieceManager::advanceTargetSequence() {
    m_sequence.advanceSequence();
    if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
        operatorLog::logNextExpectedWorkpiece(m_sequence.getExpectedType());
    }
}

void WorkpieceManager::syncSequenceAfterLostTransitToEnd(const Workpiece& wp) {
    const WpType syncType = wp.getType();
    if (syncType == WpType::UNKNOWN) {
        return;
    }

    // advanceSequence() wurde bei EJECTOR_CLEARED bereits ausgefuehrt – Rezept auf
    // den verlorenen Typ zuruecksetzen, damit das naechste erwartete Target stimmt.
    m_sequence.forceType(syncType);
    DETAIL_LOG( "[WP-Manager] Rezept nach TRANSIT_TO_END-Verlust auf Typ "
              << static_cast<int>(syncType) << " synchronisiert.");

    if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
        notifyPartnerSequenceSync(syncType);
    }
}

void WorkpieceManager::updateTargetLimitForMotor(Workpiece& wp, bool isMotorSlow) const {
    const std::uint32_t ticks = wp.getTicks();
    std::uint32_t newLimit = wp.getTargetLimit();

    switch (wp.getLocation()) {
    case WpLocation::AT_ENTRY:
        if (m_calibData.ticks_EntryB_to_EntryC_Normal > 0) {
            const std::uint32_t computed = isMotorSlow
                ? scaleNormalToSlowTicks(m_calibData.ticks_EntryB_to_EntryC_Normal)
                : m_calibData.ticks_EntryB_to_EntryC_Normal;
            newLimit = std::max(newLimit, computed);
        }
        break;

    case WpLocation::TRANSIT_TO_HEIGHT: {
        if (m_calibData.ticks_EntryC_to_HeightB_Normal == 0
            && m_calibData.ticks_EntryC_to_HeightB_Slow == 0) {
            break;
        }

        if (!isMotorSlow) {
            newLimit = std::max(newLimit, calculateTransitToHeightLimit(false));
            break;
        }

        newLimit = std::max(newLimit, calculateTransitToHeightLimit(true));
        break;
    }

    case WpLocation::TRANSIT_TO_EJECTOR: {
        const std::uint32_t normal = m_calibData.ticks_HeightC_to_EjectorB_Normal;
        if (normal == 0 && m_calibData.ticks_HeightC_to_EjectorB_Slow == 0) {
            break;
        }
        const std::uint32_t conservativeLimit = calculateEjectorTransitLimit();
        const std::uint32_t slowSegment = std::max(
            m_calibData.ticks_HeightC_to_EjectorB_Slow,
            scaleNormalToSlowTicks(normal));

        if (isMotorSlow) {
            const std::uint32_t restNormal = normal > ticks ? normal - ticks : 0U;
            const std::uint32_t restSlow = restNormal > 0
                ? scaleNormalToSlowTicks(restNormal)
                : slowSegment;
            newLimit = std::max({
                newLimit,
                conservativeLimit,
                ticks + restSlow + kTransitToEjectorSafetyTicks
            });
        } else {
            newLimit = std::max({newLimit, normal, conservativeLimit});
        }
        break;
    }

    case WpLocation::TRANSIT_TO_END: {
        if (m_calibData.ticks_EjectorC_to_EndB_Normal == 0) {
            break;
        }
        newLimit = std::max(newLimit, calculateEndTransitLimit(isMotorSlow));
        break;
    }

    case WpLocation::TRANSIT_TO_CHUTE: {
        const std::uint32_t slowFull = m_calibData.ticks_EjectorC_to_ChuteB_Slow > 0
            ? m_calibData.ticks_EjectorC_to_ChuteB_Slow
            : 150U;
        const std::uint32_t normal = (m_speedFactor > 1.0 && slowFull > 0)
            ? static_cast<std::uint32_t>(static_cast<double>(slowFull) / m_speedFactor + 0.5)
            : slowFull;
        if (normal == 0) {
            break;
        }

        if (!isMotorSlow) {
            newLimit = std::max({newLimit, normal, slowFull});
            break;
        }

        if (ticks >= normal) {
            const std::uint32_t extraSlow = slowFull > normal ? slowFull - normal : 0U;
            newLimit = std::max({newLimit, slowFull, ticks + extraSlow});
        } else {
            const std::uint32_t restSlow = scaleNormalToSlowTicks(normal - ticks);
            newLimit = std::max(newLimit, ticks + restSlow);
        }
        break;
    }

    default:
        return;
    }

    if (newLimit > 0) {
        wp.setTargetLimit(newLimit);
    }
}

void WorkpieceManager::onMotorSpeedChanged(bool isMotorSlow) {
    for (auto& wp : m_activeWorkpieces) {
        const WpLocation loc = wp.getLocation();
        if (loc != WpLocation::AT_ENTRY
            && loc != WpLocation::TRANSIT_TO_HEIGHT
            && loc != WpLocation::TRANSIT_TO_EJECTOR
            && loc != WpLocation::TRANSIT_TO_END
            && loc != WpLocation::TRANSIT_TO_CHUTE) {
            continue;
        }

        const std::uint32_t before = wp.getTargetLimit();
        updateTargetLimitForMotor(wp, isMotorSlow);
        const std::uint32_t after = wp.getTargetLimit();
        if (after != before) {
            DETAIL_LOG( "[WP-Manager] Motor " << (isMotorSlow ? "Slow" : "Fast")
                      << ": wp=" << wp.getId()
                      << " zone=" << static_cast<int>(loc)
                      << " limit " << before << " -> " << after);
        }
    }
}

bool WorkpieceManager::shouldUseSlowMotor() const {
    const std::uint32_t setSlowTick =
        calculateSetSlowTick(m_calibData.ticks_EntryC_to_HeightB_Normal);

    for (const auto& wp : m_activeWorkpieces) {
        switch (wp.getLocation()) {
        case WpLocation::TRANSIT_TO_HEIGHT:
            if (wp.getTicks() >= setSlowTick) {
                return true;
            }
            break;
        case WpLocation::AT_HEIGHT_SENSOR:
        case WpLocation::TRANSIT_TO_CHUTE:
            return true;
        default:
            break;
        }
    }
    return false;
}

void WorkpieceManager::clearAll() {
    m_activeWorkpieces.clear();
    m_sequence.resetSequence();
    resetMetalWaitState();
    resetHandoverState();
    m_ejectorBlockedWpId = -1;
    m_pendingEjectorBlockedAfterHeight = false;
    DETAIL_LOG( "[WP-Manager] Schieberegister und Rezeptur komplett zurueckgesetzt.");
}

void WorkpieceManager::resetMetalWaitState() {
    m_metalWaitWpId = -1;
    m_metalWaitTicks = 0;
    m_pendingMetalDecisionWpId = -1;
}

// ============================================================================
// AKTOR-STEUERUNG (DIREKTE UMSETZUNG LAUT DEINER LABORMETHODIK)
// ============================================================================
void WorkpieceManager::canNotPass() {
    if (!m_context || !m_context->getDispatcher()) return;

    if (m_context->getDispatcher()->isSwitch()) {
        // Weiche: geschlossen lassen -> Werkstück in Rutsche
        m_context->getDispatcher()->closeSwitch();
        m_isSwitchOpen = false;
    } else {
        // Auswerfer: 1 s ausfahren, danach wieder einfahren
        m_context->getDispatcher()->pushEjectorFor(1000);
        m_ejectorPushTicks = TICKS_ONE_SECOND;
        m_isEjectorPushed = true;
    }
}

void WorkpieceManager::canPass() {
    if (!m_context || !m_context->getDispatcher()) return;

    if (m_context->getDispatcher()->isSwitch()) {
        // Weiche: kurz oeffnen -> Werkstück geradeaus zum Bandende
        m_context->getDispatcher()->openSwitch();
        m_switchOpenTicks = kSwitchOpenHoldTicks;
        m_isSwitchOpen = true;
    }
    // Auswerfer: nichts tun -> Werkstück bleibt auf dem Hauptband
}

void WorkpieceManager::checkAndSetJunction(Workpiece* wp) {
    if (wp == nullptr) {
        return;
    }
    applyMetalIfDetected(wp);
    bool correct = false;

    if (m_myPosition == AnlagenPosition::VORN_ANLAGE_1) {
        correct = m_sequence.isCorrectType(wp->getType());
    }
    else if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
        const WpType partnerType = wp->getPartnerType();
        const WpType localType = wp->getType();
        const bool networkSaysOk =
            partnerType == WpType::UNKNOWN || partnerType == localType;
        const bool hardwareMeasuresOk = m_sequence.isCorrectType(localType);

        correct = networkSaysOk && hardwareMeasuresOk;

        if (networkSaysOk && !hardwareMeasuresOk) {
            DETAIL_LOG( "[WP-Manager] Anlage 2: Erwartet weiter Typ "
                      << static_cast<int>(m_sequence.getExpectedType())
                      << " -> Aussortieren und Anlage 1 synchronisieren.");
        } else if (!networkSaysOk) {
            DETAIL_LOG( "[WP-Manager] Anlage 2: Typabweichung zu Anlage 1 "
                      << "(Partner=" << static_cast<int>(partnerType)
                      << ", lokal=" << static_cast<int>(localType)
                      << ") -> Aussortieren.");
        }
    }
    wp->setIsCorrectInSequence(correct);

    if (correct) {
        canPass();
    } else {
        handleSortOutDecision(wp);
    }
}

void WorkpieceManager::handleSortOutDecision(Workpiece* wp) {
    if (wp == nullptr || !m_context) {
        canNotPass();
        return;
    }

    EventManager* em = m_context->getEventManager();
    ErrorHandler* errorHandler = m_context->getErrorHandler();

    if (em == nullptr || !em->isChuteFull()) {
        if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
            notifyPartnerSequenceSync();
        }
        canNotPass();
        return;
    }

    if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
        notifyPartnerSequenceSync();
        DETAIL_LOG( "[WP-Manager] Aussortierung unmoeglich – lokale Rutsche voll (WP id="
                  << wp->getId() << ").");
        if (errorHandler != nullptr) {
            errorHandler->handleEscalation(
                ErrorType::BOTH_CHUTES_FULL,
                logik::Event::CHUTE_BLOCKED,
                WpLocation::AT_EJECTOR,
                wp->getId());
        }
        return;
    }

    if (em->isPartnerChuteFull()) {
        DETAIL_LOG( "[WP-Manager] Anlage 1: Beide Rutschen voll – Aussortierung WP id="
                  << wp->getId() << " nicht moeglich.");
        if (errorHandler != nullptr) {
            errorHandler->handleEscalation(
                ErrorType::BOTH_CHUTES_FULL,
                logik::Event::CHUTE_BLOCKED,
                WpLocation::AT_EJECTOR,
                wp->getId());
        }
        return;
    }

    DETAIL_LOG( "[WP-Manager] Anlage 1: Eigene Rutsche voll – WP id=" << wp->getId()
              << " wird zur Partner-Anlage weitergeleitet.");
    wp->setDeferRejectToPartner(true);
    canPass();
}

void WorkpieceManager::handleChuteTransitFailure(Workpiece& wp) {
    ErrorHandler* errorHandler = m_context ? m_context->getErrorHandler() : nullptr;

    DETAIL_LOG( "[WP-Manager] TRANSIT_TO_CHUTE_TIMEOUT: wp=" << wp.getId()
              << " ticks=" << wp.getTicks()
              << " limit=" << wp.getTargetLimit()
              << " tolerance=" << kTimeoutToleranceTicks
              << " -> CHUTE_BLOCKED kam nicht rechtzeitig");

    if (errorHandler != nullptr) {
        errorHandler->handleEscalation(
            ErrorType::WP_LOST,
            logik::Event::UNKNOWN,
            WpLocation::TRANSIT_TO_CHUTE,
            wp.getId(),
            wp.getTicks(),
            wp.getTargetLimit(),
            kTimeoutToleranceTicks);
    }
}

bool WorkpieceManager::promoteTrailingWorkpieceAtSorterIfDue(const char* reason) {
    Workpiece* next = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR);
    if (next == nullptr) {
        return false;
    }

    const std::uint32_t limit =
        next->getTargetLimit() > 0 ? next->getTargetLimit() : calculateEjectorTransitLimit();
    const std::uint32_t threshold =
        limit > kTransitToEjectorSafetyTicks ? limit - kTransitToEjectorSafetyTicks : limit;

    if (next->getTicks() < threshold) {
        return false;
    }

    DETAIL_LOG( "[WP-Manager] " << reason << ": nachfolgendes WP "
              << next->getId()
              << " ist nahe Sortier-LS (ticks=" << next->getTicks()
              << ", threshold=" << threshold
              << ") -> EJECTOR_BLOCKED logisch nachgezogen.");

    m_ejectorLsBlocked = true;
    next->updateLocation(WpLocation::AT_EJECTOR);
    next->setTargetLimit(calculateEjectorOccupancyLimit());
    m_ejectorBlockedWpId = next->getId();

    startSorterDecisionForWp(next);
    return true;
}

Workpiece* WorkpieceManager::getLeadingWorkpieceForMetalDetection() {
    for (auto& wp : m_activeWorkpieces) {
        const WpLocation loc = wp.getLocation();
        if (loc == WpLocation::TRANSIT_TO_HEIGHT
            || loc == WpLocation::AT_HEIGHT_SENSOR
            || loc == WpLocation::TRANSIT_TO_EJECTOR
            || loc == WpLocation::AT_EJECTOR) {
            return &wp;
        }
    }
    return nullptr;
}

void WorkpieceManager::applyMetalIfDetected(Workpiece* wp) {
    if (wp == nullptr || !wp->isMetalDetected()) {
        return;
    }
    if (wp->getType() == WpType::UNKNOWN) {
        return;
    }
    const WpType before = wp->getType();
    applyMetalTypeUpgrade(wp);
    if (wp->getType() != before) {
        DETAIL_LOG( "[WP-Manager] Gespeichertes Metall-Signal angewendet "
                  << "(WP id=" << wp->getId()
                  << ", " << static_cast<int>(before)
                  << " -> " << static_cast<int>(wp->getType()) << ")."
                 );
    }
}

void WorkpieceManager::startSorterDecisionForWp(Workpiece* wp) {
    if (wp == nullptr) {
        return;
    }
    applyMetalIfDetected(wp);
    if (shouldDeferJunctionForMetal(wp)) {
        wp->setIsCorrectInSequence(false);
        m_metalWaitWpId = wp->getId();
        m_metalWaitTicks = kMetalSensorWaitTicks;
        m_pendingMetalDecisionWpId = wp->getId();
        DETAIL_LOG( "[WP-Manager] WP " << wp->getId()
                  << " wartet an der Sortiereinheit "
                  << kMetalSensorWaitTicks
                  << " Takte auf Metall-Entscheidung."
                 );
        return;
    }
    checkAndSetJunction(wp);
}

Workpiece* WorkpieceManager::findWorkpieceById(int id) {
    for (auto& wp : m_activeWorkpieces) {
        if (wp.getId() == id) {
            return &wp;
        }
    }
    return nullptr;
}

bool WorkpieceManager::hasEjectorSorter() const {
    return m_context != nullptr
        && m_context->getDispatcher() != nullptr
        && !m_context->getDispatcher()->isSwitch();
}

bool WorkpieceManager::shouldDeferJunctionForMetal(const Workpiece* wp) const {
    if (wp == nullptr) {
        return false;
    }
    const WpType type = wp->getType();
    const WpType expected = m_sequence.getExpectedType();
    return (type == WpType::TALL && expected == WpType::TALL_METAL)
        || (type == WpType::TALL_DRILLED && expected == WpType::TALL_DRILLED_METAL);
}

void WorkpieceManager::applyMetalTypeUpgrade(Workpiece* wp) {
    if (wp == nullptr) {
        return;
    }
    if (wp->getType() == WpType::TALL_DRILLED) {
        wp->setMeasuredType(WpType::TALL_DRILLED_METAL);
    } else if (wp->getType() == WpType::TALL) {
        wp->setMeasuredType(WpType::TALL_METAL);
    }
}

} // namespace logik
