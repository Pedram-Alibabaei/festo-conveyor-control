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

int Workpiece::s_next_id = 1;

namespace logik {

namespace {
constexpr double kWorkpieceDiameterCm = 4.0;
constexpr double kWorstCaseExtraPathCm = 6.9;
constexpr double kSetSlowFactor = 0.65;
constexpr std::uint32_t kMinSetSlowTick = 20;
constexpr std::uint32_t kTransitToHeightSafetyTicks = 50;
constexpr std::uint32_t kTimeoutToleranceTicks = 30;
/** Weiche laenger offen (10ms-Takte); 150 = 1,5 s. */
constexpr int kSwitchOpenHoldTicks = 150;

std::uint32_t calculateWorstCaseExtraTicks(std::uint32_t entryBlockedTicks) {
    if (entryBlockedTicks == 0) {
        return 0;
    }

    const double ticksPerCm = static_cast<double>(entryBlockedTicks) / kWorkpieceDiameterCm;
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
      m_isSwitchOpen(false),
      m_switchOpenTicks(0),
      m_isEjectorPushed(false),
      m_ejectorPushTicks(0),
      TICKS_ONE_SECOND(100)
{}

void WorkpieceManager::setCalibrationData(const CalibrationData& data) {
    m_calibData = data;

}

void WorkpieceManager::addWorkpiece() {
    m_activeWorkpieces.emplace_back();
    Workpiece& newWp = m_activeWorkpieces.back();

    newWp.markEntryBlockedSeen();
    newWp.setTargetLimit(m_calibData.ticks_EntryC_to_HeightB_Normal);
    std::cout << "[WP-Manager] CREATE WP lokal: id=" << newWp.getId()
              << " trigger=ENTRY_BLOCKED zone=AT_ENTRY targetLimit="
              << m_calibData.ticks_EntryC_to_HeightB_Normal << std::endl;
}

void WorkpieceManager::addNetworkWorkpiece(int id, WpType type) {
    m_activeWorkpieces.emplace_back();
    Workpiece& netWp = m_activeWorkpieces.back();

    netWp.setId(id);
    netWp.setType(type);

    netWp.updateLocation(WpLocation::AT_ENTRY);
    netWp.setTargetLimit(m_calibData.ticks_EntryC_to_HeightB_Normal);

    bool matchesLocalSequence = m_sequence.isCorrectType(type);
    netWp.setIsExpectedFromNetwork(matchesLocalSequence);

    std::cout << "[WP-Manager] Netzwerk-Teil von Anlage 1 uebernommen. ID: " << netWp.getId()
              << ", Typ-ID: " << static_cast<int>(netWp.getType()) << " -> Zone: At_Entry" << std::endl;
}

void WorkpieceManager::storePendingNetworkWorkpiece(int id, WpType type) {
    m_pendingNetworkWpId = id;
    m_pendingNetworkWpType = type;
    m_hasPendingNetworkWp = true;
    m_handoverReceiveActive = true;
    std::cout << "[WP-Manager] Anlage 2: WP-Info zwischengespeichert id=" << id
              << " type=" << static_cast<int>(type)
              << " (wartet auf ENTRY_BLOCKED)." << std::endl;
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

    if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2
        && m_context != nullptr && m_context->getDispatcher() != nullptr) {
        std::cout << "[WP-Manager] Anlage 2: Uebergabe erfolgreich -> melde Anlage 1."
                  << std::endl;
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
}

bool WorkpieceManager::canAcceptNetworkHandover() {
    if (m_myPosition != AnlagenPosition::HINTEN_ANLAGE_2) {
        return false;
    }
    if (!m_isAnlage2Ready) {
        return false;
    }
    if (m_hasPendingNetworkWp || getFirstWorkpieceAt(WpLocation::AT_ENTRY) != nullptr) {
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
    if (!canAcceptNetworkHandover()) {
        std::cout << "[WP-Manager] Anlage 2: Uebergabe-Anfrage abgelehnt (nicht bereit)."
                  << std::endl;
        return;
    }
    if (m_context == nullptr || m_context->getDispatcher() == nullptr) {
        return;
    }
    std::cout << "[WP-Manager] Anlage 2: Bereit fuer WP-Uebergabe -> READY_TO_RECEIVE."
              << std::endl;
    m_context->getDispatcher()->readyToReceiveWp();
}

void WorkpieceManager::onPartnerReadyToReceive() {
    if (m_myPosition != AnlagenPosition::VORN_ANLAGE_1) {
        return;
    }
    if (!m_awaitingHandoverReady) {
        std::cout << "[WP-Manager] Anlage 1: READY_TO_RECEIVE ohne offene Anfrage ignoriert."
                  << std::endl;
        return;
    }
    Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_END);
    if (wp == nullptr) {
        std::cout << "[WP-Manager] Anlage 1: READY_TO_RECEIVE aber kein WP am Bandende."
                  << std::endl;
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
    std::cout << "[WP-Manager] Anlage 1: Partner bereit -> sende WP-Info id=" << wp->getId()
              << " und starte Band." << std::endl;
    m_context->getDispatcher()->sendWorkpieceInfo(wp->getId(), wp->getType());
    m_context->processEvent(logik::Event::MOTOR_RUN);
    m_awaitingHandoverReady = false;
    m_handoverInfoSent = true;
    m_handoverMotorActive = true;
}

void WorkpieceManager::onPartnerHandoverComplete() {
    if (m_myPosition != AnlagenPosition::VORN_ANLAGE_1) {
        return;
    }
    if (!m_handoverMotorActive) {
        std::cout << "[WP-Manager] Anlage 1: HANDOVER_COMPLETE ohne aktive Uebergabe ignoriert."
                  << std::endl;
        return;
    }
    std::cout << "[WP-Manager] Anlage 1: Partner meldet ENTRY_BLOCKED -> Motor aus."
              << std::endl;
    m_handoverMotorActive = false;
    m_handoverInfoSent = false;
    if (m_context != nullptr) {
        m_context->processEvent(logik::Event::MOTOR_STOP);
    }
}

void WorkpieceManager::onPartnerHandoverWpLost() {
    if (m_myPosition != AnlagenPosition::VORN_ANLAGE_1) {
        return;
    }
    if (!m_handoverMotorActive) {
        return;
    }
    std::cout << "[WP-Manager] Anlage 1: Partner meldet WP verloren bei Uebergabe -> Motor aus."
              << std::endl;
    m_handoverMotorActive = false;
    m_handoverInfoSent = false;
    m_awaitingHandoverReady = false;
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
    std::cout << "[WP-Manager] Anlage 2: Uebergabe fehlgeschlagen -> melde Anlage 1."
              << std::endl;
    m_context->getDispatcher()->sendHandoverWpLost();
}

// ============================================================================
// CENTRAL TIMER TICK (ZENTRALER HERZSCHLAG MIT TRANSIT-WATCHDOGS)
// ============================================================================
void WorkpieceManager::takt(bool isMotorSlow) {
    for (auto& wp : m_activeWorkpieces) {

        if (wp.getLocation() == WpLocation::AT_HEIGHT_SENSOR ||
            wp.getLocation() == WpLocation::AT_EJECTOR ||
            wp.getLocation() == WpLocation::AT_CHUTE ||
            wp.getLocation() == WpLocation::AT_END) {
            continue;
        }

        wp.incrementTicks();
        updateTargetLimitForMotor(wp, isMotorSlow);

        switch (wp.getLocation()) {
            case WpLocation::AT_ENTRY:
                if (wp.hasEntryBlockedSeen()
                    && wp.getTargetLimit() > 0
                    && wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    std::cout << "[WP-Manager] ENTRY_TIMEOUT: wp=" << wp.getId()
                              << " ticks=" << wp.getTicks()
                              << " limit=" << wp.getTargetLimit()
                              << " tolerance=" << kTimeoutToleranceTicks << " -> WP_LOST" << std::endl;
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::WP_LOST, logik::Event::ENTRY_BLOCKED,
                            WpLocation::AT_ENTRY, wp.getId()
                        );
                    }
                }
                break;

            case WpLocation::TRANSIT_TO_HEIGHT:
                {
                const std::uint32_t setSlowTick =
                    calculateSetSlowTick(m_calibData.ticks_EntryC_to_HeightB_Normal);

                if (wp.getTicks() == setSlowTick) {
                    std::cout << "[WP-Manager] SEND SET_SLOW: wp=" << wp.getId()
                              << " location=TRANSIT_TO_HEIGHT ticks=" << wp.getTicks()
                              << " threshold=" << setSlowTick
                              << " factor=" << kSetSlowFactor
                              << " normalCalib=" << m_calibData.ticks_EntryC_to_HeightB_Normal
                              << std::endl;
                    m_context->processEvent(logik::Event::SET_SLOW);

                    const double restAnteilNormal =
                        m_calibData.ticks_EntryC_to_HeightB_Normal > 0
                            ? std::max(0.0, 1.0 - (static_cast<double>(setSlowTick)
                                / static_cast<double>(m_calibData.ticks_EntryC_to_HeightB_Normal)))
                            : 0.0;
                    std::uint32_t restTakteSlow =
                        static_cast<std::uint32_t>(restAnteilNormal
                            * static_cast<double>(m_calibData.ticks_EntryC_to_HeightB_Slow));

                    const std::uint32_t worstCaseExtraTicksNormal =
                        wp.getTargetLimit() > m_calibData.ticks_EntryC_to_HeightB_Normal
                            ? wp.getTargetLimit() - m_calibData.ticks_EntryC_to_HeightB_Normal
                            : 0;
                    const double slowFactor =
                        m_calibData.ticks_EntryC_to_HeightB_Normal > 0
                            ? static_cast<double>(m_calibData.ticks_EntryC_to_HeightB_Slow)
                                / static_cast<double>(m_calibData.ticks_EntryC_to_HeightB_Normal)
                            : 1.0;
                    const std::uint32_t worstCaseExtraTicksSlow =
                        static_cast<std::uint32_t>(
                            (static_cast<double>(worstCaseExtraTicksNormal) * slowFactor) + 0.5);
                    const std::uint32_t mixedSpeedTarget =
                        setSlowTick + restTakteSlow + worstCaseExtraTicksSlow
                        + kTransitToHeightSafetyTicks;
                    const std::uint32_t conservativeSlowTarget =
                        m_calibData.ticks_EntryC_to_HeightB_Slow
                        + worstCaseExtraTicksSlow
                        + kTransitToHeightSafetyTicks;
                    const std::uint32_t targetLimit =
                        std::max(mixedSpeedTarget, conservativeSlowTarget);

                    std::cout << "[WP-Manager] SET_SLOW target neu: wp=" << wp.getId()
                              << " setSlowTick=" << setSlowTick
                              << " restSlow=" << restTakteSlow
                              << " extraNormal=" << worstCaseExtraTicksNormal
                              << " extraSlow=" << worstCaseExtraTicksSlow
                              << " safety=" << kTransitToHeightSafetyTicks
                              << " mixedTarget=" << mixedSpeedTarget
                              << " conservativeSlowTarget=" << conservativeSlowTarget
                              << " targetLimit=" << targetLimit
                              << std::endl;

                    wp.setTargetLimit(targetLimit);
                }
                }

                // FEHLER: WP verloren vor Höhenmessung -> Zone übergeben
                if (wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    std::cout << "[WP-Manager] TRANSIT_TO_HEIGHT_TIMEOUT: wp="
                              << wp.getId()
                              << " ticks=" << wp.getTicks()
                              << " targetLimit=" << wp.getTargetLimit()
                              << " tolerance=" << kTimeoutToleranceTicks << " -> WP_LOST" << std::endl;
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::WP_LOST, logik::Event::UNKNOWN, wp.getLocation(), wp.getId()
                        );
                    }
                }
                break;

            case WpLocation::TRANSIT_TO_EJECTOR:
                if (wp.getTargetLimit() > 0
                    && wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    std::cout << "[WP-Manager] TRANSIT_TO_EJECTOR_TIMEOUT: wp="
                              << wp.getId()
                              << " ticks=" << wp.getTicks()
                              << " limit=" << wp.getTargetLimit()
                              << " tolerance=" << kTimeoutToleranceTicks << " -> WP_LOST" << std::endl;
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::WP_LOST, logik::Event::UNKNOWN, wp.getLocation(), wp.getId()
                        );
                    }
                }
                break;

            case WpLocation::TRANSIT_TO_CHUTE:
                if (wp.getTargetLimit() > 0
                    && wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    std::cout << "[WP-Manager] TRANSIT_TO_CHUTE_TIMEOUT: wp="
                              << wp.getId()
                              << " ticks=" << wp.getTicks()
                              << " limit=" << wp.getTargetLimit()
                              << " tolerance=" << kTimeoutToleranceTicks << " -> STUCK" << std::endl;
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::STUCK, logik::Event::UNKNOWN, wp.getLocation(), wp.getId()
                        );
                    }
                }
                break;

            case WpLocation::TRANSIT_TO_END:
                if (wp.getTargetLimit() > 0
                    && wp.getTicks() > wp.getTargetLimit() + kTimeoutToleranceTicks) {
                    std::cout << "[WP-Manager] TRANSIT_TO_END_TIMEOUT: wp="
                              << wp.getId()
                              << " ticks=" << wp.getTicks()
                              << " limit=" << wp.getTargetLimit()
                              << " tolerance=" << kTimeoutToleranceTicks << " -> WP_LOST" << std::endl;
                    if (wp.isCorrectInSequence()) {
                        m_sequence.forceType(wp.getType());
                    }

                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::WP_LOST, logik::Event::UNKNOWN, wp.getLocation(), wp.getId()
                        );
                    }
                }
                break;

            default:
                break;
        }
    }

    // --- AUTOMATISCHE AKTOR-RUECKSETZUNG IM ZEITTAKT (1 s) ---
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
}

// ============================================================================
// SENSOR-SIGNAL VERARBEITUNG
// ============================================================================
void WorkpieceManager::handleSensorSignal(logik::Event e, WpType typeFromHal) {

    switch (e) {
        // --- 1. ZONE: EINLAUF ---
        case logik::Event::ENTRY_BLOCKED: {
            if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2
                && getFirstWorkpieceAt(WpLocation::AT_ENTRY) == nullptr
                && m_hasPendingNetworkWp) {
                commitPendingNetworkWorkpieceAtEntry();
            }
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_ENTRY);
            if (wp != nullptr) {
                if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
                    auto* eventManager = m_context ? m_context->getEventManager() : nullptr;
                    if (eventManager == nullptr || !eventManager->isMotorOn()) {
                        std::cout << "[WP-Manager] Anlage 2: ENTRY_BLOCKED vor "
                                  << "Netzwerk-Uebergabe/MOTOR_RUN -> FEHLER." << std::endl;
                        if (m_context && m_context->getErrorHandler()) {
                            m_context->getErrorHandler()->handleEscalation(
                                ErrorType::UNEXPECTED_EVENT, e, WpLocation::AT_ENTRY, wp->getId());
                        }
                        break;
                    }
                }
                if (!wp->hasEntryBlockedSeen()) {
                    wp->markEntryBlockedSeen();
                    std::cout << "[WP-Manager] Netzwerk-WP " << wp->getId()
                              << " erreicht ENTRY_BLOCKED physisch -> Entry-Timer startet."
                              << std::endl;
                } else {
                    std::cout << "[WP-Manager] Ignoriere doppelte ENTRY_BLOCKED-Flanke fuer WP "
                              << wp->getId() << "." << std::endl;
                }
            } else if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
                std::cout << "[WP-Manager] Anlage 2: ENTRY_BLOCKED ohne WP-Info von "
                          << "Anlage 1 -> FEHLER." << std::endl;
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(
                        ErrorType::UNEXPECTED_EVENT, e, WpLocation::AT_ENTRY);
                }
            } else {
                std::cout << "[WP-Manager] ENTRY_BLOCKED ohne aktives WP -> ignoriert." << std::endl;
            }
            break;
        }

        case logik::Event::ENTRY_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_ENTRY);
            if (wp != nullptr) {
                const std::uint32_t entryBlockedTicks = wp->getTicks();
                const std::uint32_t worstCaseExtraTicks =
                    calculateWorstCaseExtraTicks(entryBlockedTicks);
                const std::uint32_t worstCaseTarget =
                    m_calibData.ticks_EntryC_to_HeightB_Normal + worstCaseExtraTicks;

                std::cout << "[HAL-Signal] ENTRY_CLEARED fuer WP " << wp->getId()
                          << " -> Transit zum HS. Erwartet EntryC->HeightB Normal="
                          << m_calibData.ticks_EntryC_to_HeightB_Normal
                          << " Takte, Slow=" << m_calibData.ticks_EntryC_to_HeightB_Slow
                          << " Takte." << std::endl;
                std::cout << "[WP-Manager] ENTRY_LS_DAUER: wp=" << wp->getId()
                          << " blockedTicks=" << entryBlockedTicks
                          << " (4cm) extraWorstCaseTicks=" << worstCaseExtraTicks
                          << " targetLimitWorstCase=" << worstCaseTarget
                          << std::endl;

                wp->updateLocation(WpLocation::TRANSIT_TO_HEIGHT);
                wp->setTargetLimit(worstCaseTarget);
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

        // --- 2. ZONE: HÖHENMESSUNG & METALLERKENNUNG ---
        case logik::Event::HEIGHT_BLOCKED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_HEIGHT);
            if (wp != nullptr) {
                const auto consumedTicks = wp->getTicks();
                std::cout << "[WP-Manager] HEIGHT_BLOCKED: wp=" << wp->getId()
                          << " verbrauchteTakte=" << consumedTicks
                          << " (ca. " << (consumedTicks * 10) << "ms)"
                          << " kalibriertNormal=" << m_calibData.ticks_EntryC_to_HeightB_Normal
                          << " kalibriertSlow=" << m_calibData.ticks_EntryC_to_HeightB_Slow
                          << " targetLimit=" << wp->getTargetLimit()
                          << std::endl;
                std::cout << "[HAL-Signal] WP " << wp->getId()
                          << " an Hoehenmessung eingetroffen." << std::endl;
                wp->updateLocation(WpLocation::AT_HEIGHT_SENSOR);
            } else if (getFirstWorkpieceAt(WpLocation::AT_HEIGHT_SENSOR) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR) != nullptr) {
                std::cout << "[WP-Manager] Ignoriere doppelte/spaete HEIGHT_BLOCKED-Flanke." << std::endl;
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

         case logik::Event::HEIGHT_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_HEIGHT_SENSOR);
            if (wp != nullptr) {
                wp->setType(typeFromHal);
                std::cout << "[HAL-Signal] Messung beendet für WP " << wp->getId()
                          << ". Finaler Typ gesetzt: " << static_cast<int>(wp->getType())
                          << " -> Transit zur Weiche." << std::endl;

                wp->updateLocation(WpLocation::TRANSIT_TO_EJECTOR);
                {
                    const std::uint32_t normal = m_calibData.ticks_HeightC_to_EjectorB_Normal;
                    const std::uint32_t slow = m_calibData.ticks_HeightC_to_EjectorB_Slow;
                    const std::uint32_t slowFull = slow > 0
                        ? slow
                        : scaleNormalToSlowTicks(normal);
                    wp->setTargetLimit(std::max(normal, slowFull));
                }
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

        case logik::Event::HEIGHT_ERROR: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_HEIGHT_SENSOR);
            int id = wp ? wp->getId() : -1;

            if (m_context && m_context->getErrorHandler()) {
                m_context->getErrorHandler()->handleEscalation(ErrorType::HEIGHT_ERROR, e, WpLocation::AT_HEIGHT_SENSOR, id);
            }
            break;
        }

        case logik::Event::METAL_DETECTED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR);
            if (wp != nullptr) {
                if (wp->getType() == WpType::TALL_DRILLED) {
                    wp->setType(WpType::TALL_DRILLED_METAL);
                }
                else if (wp->getType() == WpType::TALL) {
                    wp->setType(WpType::TALL_METAL);
                }
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
                std::cout << "[HAL-Signal] WP " << wp->getId() << " erreicht physisch die Sortierweiche." << std::endl;
                wp->updateLocation(WpLocation::AT_EJECTOR);
                checkAndSetJunction(wp);
            } else if (getFirstWorkpieceAt(WpLocation::AT_EJECTOR) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_END) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_CHUTE) != nullptr) {
                std::cout << "[WP-Manager] Ignoriere doppelte/spaete EJECTOR_BLOCKED-Flanke." << std::endl;
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

        case logik::Event::EJECTOR_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_EJECTOR);
            if (wp != nullptr) {
                if (wp->isCorrectInSequence()) {
                    wp->updateLocation(WpLocation::TRANSIT_TO_END);
                    {
                        const std::uint32_t normal = m_calibData.ticks_EjectorC_to_EndB_Normal;
                        const std::uint32_t slowFull = normal > 0
                            ? scaleNormalToSlowTicks(normal)
                            : 0U;
                        wp->setTargetLimit(std::max(normal, slowFull));
                    }
                    m_sequence.advanceSequence();
                } else {
                    wp->updateLocation(WpLocation::TRANSIT_TO_CHUTE);
                    const std::uint32_t chuteLimit = m_calibData.ticks_EjectorC_to_ChuteB_Slow > 0
                        ? m_calibData.ticks_EjectorC_to_ChuteB_Slow
                        : 150U;
                    wp->setTargetLimit(chuteLimit);
                }
            } else if (getFirstWorkpieceAt(WpLocation::TRANSIT_TO_END) != nullptr
                       || getFirstWorkpieceAt(WpLocation::TRANSIT_TO_CHUTE) != nullptr) {
                std::cout << "[WP-Manager] Ignoriere doppelte/spaete EJECTOR_CLEARED-Flanke." << std::endl;
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

                    if (!partnerOk) {
                        if (wp->isCorrectInSequence()) {
                            m_sequence.forceType(wp->getType());
                        }
                        std::cout << "[WP-Manager] Anlage 1: Bandende belegt, Partner nicht "
                                  << "bereit -> keine Uebergabe-Anfrage." << std::endl;
                    } else if (m_context && m_context->getDispatcher()) {
                        std::cout << "[WP-Manager] Anlage 1: WP id=" << wp->getId()
                                  << " am Bandende -> frage Anlage 2 (REQ_TO_GIVE)."
                                  << std::endl;
                        m_context->getDispatcher()->RequestToReceiveWp();
                        m_awaitingHandoverReady = true;
                    }
                } else if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
                    setAnlage2Ready(false);
                    std::cout << "[WP-Manager] Anlage 2: WP id=" << wp->getId()
                              << " am Bandende -> Motor stoppt, keine Uebergabe bis END_CLEARED."
                              << std::endl;
                }
            } else if (getFirstWorkpieceAt(WpLocation::AT_END) != nullptr) {
                std::cout << "[WP-Manager] Ignoriere doppelte END_BLOCKED-Flanke." << std::endl;
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
                        std::cout << "[WP-Manager] Anlage 1: END_CLEARED vor Partner-Bestaetigung."
                                  << std::endl;
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
                    std::cout << "[WP-Manager] Anlage 2: Bandende frei -> wieder bereit fuer Uebergabe."
                              << std::endl;
                }
                removeWorkpiece(wp->getId());
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
                wp->updateLocation(WpLocation::AT_CHUTE);
                const bool onlyWpNowInChute = m_activeWorkpieces.size() == 1;
                if (onlyWpNowInChute && m_context != nullptr) {
                    std::cout << "[WP-Manager] Letztes WP in Rutsche -> BAND_EMPTY bei CHUTE_BLOCKED."
                              << std::endl;
                    m_context->processEvent(logik::Event::BAND_EMPTY);
                }
            } else {
                std::cout << "[WP-Manager] CHUTE_BLOCKED ohne WP ignoriert (nur Fuellstand/Warnung)." << std::endl;
            }
            break;
        }

        case logik::Event::CHUTE_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_CHUTE);
            if (wp != nullptr) {
                removeWorkpiece(wp->getId());
            } else {
                std::cout << "[WP-Manager] CHUTE_CLEARED ohne WP ignoriert (nur Fuellstand/Warnung)." << std::endl;
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

void WorkpieceManager::updateTargetLimitForMotor(Workpiece& wp, bool isMotorSlow) const {
    const std::uint32_t ticks = wp.getTicks();
    std::uint32_t newLimit = wp.getTargetLimit();

    switch (wp.getLocation()) {
    case WpLocation::AT_ENTRY:
        if (m_calibData.ticks_EntryB_to_EntryC_Normal > 0) {
            newLimit = isMotorSlow
                ? scaleNormalToSlowTicks(m_calibData.ticks_EntryB_to_EntryC_Normal)
                : m_calibData.ticks_EntryB_to_EntryC_Normal;
        }
        break;

    case WpLocation::TRANSIT_TO_HEIGHT: {
        const std::uint32_t normal = m_calibData.ticks_EntryC_to_HeightB_Normal;
        if (normal == 0) {
            break;
        }
        const std::uint32_t slowFull = m_calibData.ticks_EntryC_to_HeightB_Slow > 0
            ? m_calibData.ticks_EntryC_to_HeightB_Slow
            : scaleNormalToSlowTicks(normal);

        if (!isMotorSlow) {
            if (newLimit == 0) {
                newLimit = normal;
            }
            break;
        }

        const std::uint32_t setSlowTick = calculateSetSlowTick(normal);
        if (ticks >= setSlowTick || newLimit > normal) {
            newLimit = std::max(newLimit, slowFull + kTransitToHeightSafetyTicks);
        } else {
            const std::uint32_t restNormal = normal > ticks ? normal - ticks : 0U;
            const std::uint32_t restSlow = scaleNormalToSlowTicks(restNormal);
            newLimit = std::max(newLimit, ticks + restSlow + kTransitToHeightSafetyTicks);
        }
        break;
    }

    case WpLocation::TRANSIT_TO_EJECTOR: {
        const std::uint32_t normal = m_calibData.ticks_HeightC_to_EjectorB_Normal;
        if (normal == 0 && m_calibData.ticks_HeightC_to_EjectorB_Slow == 0) {
            break;
        }
        const std::uint32_t slowFull = m_calibData.ticks_HeightC_to_EjectorB_Slow > 0
            ? m_calibData.ticks_HeightC_to_EjectorB_Slow
            : scaleNormalToSlowTicks(normal);
        const std::uint32_t conservativeLimit = std::max(normal, slowFull);

        if (isMotorSlow) {
            newLimit = std::max(newLimit, conservativeLimit);
            if (ticks > 0 && normal > 0 && ticks < normal) {
                const std::uint32_t restSlow = scaleNormalToSlowTicks(normal - ticks);
                newLimit = std::max(newLimit, ticks + restSlow);
            }
        } else {
            // Fast: mindestens max(Normal, Slow) wie zuvor (Normal allein kann zu klein sein)
            newLimit = std::max({newLimit, normal, conservativeLimit});
        }
        break;
    }

    case WpLocation::TRANSIT_TO_END: {
        const std::uint32_t normal = m_calibData.ticks_EjectorC_to_EndB_Normal;
        if (normal == 0) {
            break;
        }
        const std::uint32_t slowFull = scaleNormalToSlowTicks(normal);
        const std::uint32_t conservativeLimit = std::max(normal, slowFull);

        if (!isMotorSlow) {
            newLimit = std::max(newLimit, conservativeLimit);
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
    resetHandoverState();
    std::cout << "[WP-Manager] Schieberegister und Rezeptur komplett zurueckgesetzt." << std::endl;
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
        // Auswerfer: ausfahren -> Werkstück aussortieren
        m_context->getDispatcher()->pushEjector();
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
    bool correct = false;

    if (m_myPosition == AnlagenPosition::VORN_ANLAGE_1) {
        correct = m_sequence.isCorrectType(wp->getType());
    }
    else if (m_myPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
        bool networkSaysOk = wp->isExpectedFromNetwork();
        bool hardwareMeasuresOk = m_sequence.isCorrectType(wp->getType());

        correct = networkSaysOk && hardwareMeasuresOk;

        if (networkSaysOk && !hardwareMeasuresOk) {
            WpType missingType = m_sequence.getExpectedType();
            m_context->getDispatcher()->updateWpListe(missingType);

        }
    }
    wp->setIsCorrectInSequence(correct);

    if (correct) {
        canPass();
    } else {
        canNotPass();
    }
}

} // namespace logik
