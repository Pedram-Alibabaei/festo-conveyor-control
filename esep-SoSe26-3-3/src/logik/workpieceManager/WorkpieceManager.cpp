/**
 * @file WorkpieceManager.cpp
 * @brief Implementierung des zeittaktbasierten Schieberegisters mit zonenbasierter Fehlereskalation.
 *
 * SoSe2026 Team 3.3
 */

#include "WorkpieceManager.hpp"
#include "logik/logik.hpp"
#include "logik/errorHandler/errorHandler.hpp"
#include <algorithm>
#include <iostream>
#include "shared/logikEvent.hpp"

int Workpiece::s_next_id = 1;

namespace logik {

WorkpieceManager::WorkpieceManager(Logik* context)
    : m_context(context),
      m_speedFactor(1.0),
      m_myPosition(AnlagenPosition::VORN_ANLAGE_1),
      m_isAnlage2Ready(true),
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

    newWp.setTargetLimit(m_calibData.ticks_EntryC_to_HeightB_Normal);
    std::cout << "[WP-Manager] Digitaler Zwilling lokal erzeugt. ID: " << newWp.getId() << " -> Zone: At_Entry" << std::endl;
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

// ============================================================================
// CENTRAL TIMER TICK (ZENTRALER HERZSCHLAG MIT TRANSIT-WATCHDOGS)
// ============================================================================
void WorkpieceManager::takt(bool isMotorSlow) {
    for (auto& wp : m_activeWorkpieces) {

        if (wp.getLocation() == WpLocation::AT_ENTRY ||
            wp.getLocation() == WpLocation::AT_HEIGHT_SENSOR ||
            wp.getLocation() == WpLocation::AT_EJECTOR ||
            wp.getLocation() == WpLocation::AT_CHUTE ||
            wp.getLocation() == WpLocation::AT_END) {
            continue;
        }

        wp.incrementTicks();

        switch (wp.getLocation()) {

            case WpLocation::TRANSIT_TO_HEIGHT:
                if (wp.getTicks() == 200) {
                    std::cout << "[WP-Manager] WP " << wp.getId() << " erreicht Takt 200. Erzeuge SET_SLOW." << std::endl;
                    m_context->processEvent(logik::Event::SET_SLOW);

                    double restAnteilNormal = 1.0 - (200.0 / (double)m_calibData.ticks_EntryC_to_HeightB_Normal);
                    std::uint32_t restTakteSlow = restAnteilNormal * (double)m_calibData.ticks_EntryC_to_HeightB_Slow;

                    wp.setTargetLimit(200 + restTakteSlow);
                }

                // FEHLER: WP verloren vor Höhenmessung -> Zone übergeben
                if (wp.getTicks() > wp.getTargetLimit() + 30) {
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::WP_LOST, logik::Event::UNKNOWN, wp.getLocation(), wp.getId()
                        );
                    }
                }
                break;

            case WpLocation::TRANSIT_TO_EJECTOR:
                {
                    std::uint32_t currentLimit = isMotorSlow ? m_calibData.ticks_HeightC_to_EjectorB_Slow
                                                             : m_calibData.ticks_HeightC_to_EjectorB_Normal;

                    // FEHLER: WP verloren vor Weiche -> Zone übergeben
                    if (wp.getTicks() > currentLimit + 30) {
                        if (m_context && m_context->getErrorHandler()) {
                            m_context->getErrorHandler()->handleEscalation(
                                ErrorType::WP_LOST, logik::Event::UNKNOWN, wp.getLocation(), wp.getId()
                            );
                        }
                    }
                }
                break;

            case WpLocation::TRANSIT_TO_CHUTE:
                // FEHLER: WP verklemmt in der Rutsche -> Zone übergeben
                if (wp.getTicks() > wp.getTargetLimit()) {
                    if (m_context && m_context->getErrorHandler()) {
                        m_context->getErrorHandler()->handleEscalation(
                            ErrorType::STUCK, logik::Event::UNKNOWN, wp.getLocation(), wp.getId()
                        );
                    }
                }
                break;

            case WpLocation::TRANSIT_TO_END:
                // FEHLER: WP verloren auf dem Weg zum Bandende -> Zone übergeben
                if (wp.getTicks() > wp.getTargetLimit() + 30) {
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
            if (m_context && m_context->getErrorHandler()) {
                m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
            }
            break;
        }

        case logik::Event::ENTRY_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_ENTRY);
            if (wp != nullptr) {
                std::cout << "[HAL-Signal] ENTRY_CLEARED fuer WP " << wp->getId() << " -> Transit zum HS." << std::endl;
                wp->updateLocation(WpLocation::TRANSIT_TO_HEIGHT);
                wp->setTargetLimit(m_calibData.ticks_EntryC_to_HeightB_Normal);
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
                std::cout << "[HAL-Signal] WP " << wp->getId() << " an Hoehenmessung eingetroffen." << std::endl;
                wp->updateLocation(WpLocation::AT_HEIGHT_SENSOR);
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
                    wp->setTargetLimit(m_calibData.ticks_EjectorC_to_EndB_Normal);
                    m_sequence.advanceSequence();
                } else {
                    wp->updateLocation(WpLocation::TRANSIT_TO_CHUTE);
                    wp->setTargetLimit(150);
                }
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

                if (m_myPosition == AnlagenPosition::VORN_ANLAGE_1 && !m_isAnlage2Ready) {
                    if (wp->isCorrectInSequence()) {
                        m_sequence.forceType(wp->getType());
                    }
                    //m_context->processEvent(logik::Event::MOTOR_STOP);
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
                    if (m_isAnlage2Ready) {
                        m_context->getDispatcher()->sendWorkpieceInfo(wp->getId(), wp->getType());
                    } else {
                        if (wp->isCorrectInSequence()) {
                            m_sequence.forceType(wp->getType());
                        }
                        if (m_context && m_context->getErrorHandler()) {
                            m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                        }
                        break;
                    }
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
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
            }
            break;
        }

        case logik::Event::CHUTE_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_CHUTE);
            if (wp != nullptr) {
                removeWorkpiece(wp->getId());
            } else {
                if (m_context && m_context->getErrorHandler()) {
                    m_context->getErrorHandler()->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
                }
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
}

void WorkpieceManager::clearAll() {
    m_activeWorkpieces.clear();
    m_sequence.resetSequence();
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
        m_switchOpenTicks = TICKS_ONE_SECOND;
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
