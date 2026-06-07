/**
 * @file WorkpieceManager.cpp
 * @brief Implementierung des zeittaktbasierten Schieberegisters mit Netzwerkkoppelung.
 *
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#include "workpieceManager.hpp"
#include "logik/logik.hpp" // Enthält die konkrete Logik, processEvent sowie Zugriff auf den Dispatcher
#include <algorithm>
#include <iostream>

int Workpiece::s_next_id = 1;

namespace logik {

WorkpieceManager::WorkpieceManager(Logik* context)
    : m_context(context),
      m_speedFactor(1.0),
      m_myPosition(AnlagenPosition::VORN_ANLAGE_1), // Standardmäßig als Anlage 1 starten
      m_isAnlage2Ready(true)                        // Standardmäßig startbereit
{}

void WorkpieceManager::setCalibrationData(const CalibrationData& data) {
    m_calibData = data;
}

void WorkpieceManager::addWorkpiece() {
    m_activeWorkpieces.emplace_back();
    Workpiece& newWp = m_activeWorkpieces.back();

    // Initiales Limit für das erste Segment (Normalfahrt) eintragen
    newWp.setTargetLimit(m_calibData.ticks_EntryC_to_HeightB_Normal);

    std::cout << "[WP-Manager] Digitaler Zwilling lokal erzeugt. ID: " << newWp.getId() << " -> Zone: At_Entry" << std::endl;
}

void WorkpieceManager::addNetworkWorkpiece(int id, WpType type) {
    m_activeWorkpieces.emplace_back();
    Workpiece& netWp = m_activeWorkpieces.back();

    netWp.setId(id);
    netWp.setType(type);

    // Das Teil startet logisch direkt am Einlauf von Anlage 2
    netWp.updateLocation(WpLocation::AT_ENTRY);
    netWp.setTargetLimit(m_calibData.ticks_EntryC_to_HeightB_Normal);

    // Rezept-Check bei Eintritt auf Anlage 2 im Werkstück einfrieren
    bool matchesLocalSequence = m_sequence.isCorrectType(type);
    netWp.setIsExpectedFromNetwork(matchesLocalSequence);

    std::cout << "[WP-Manager] Netzwerk-Teil von Anlage 1 uebernommen. ID: " << netWp.getId()
              << ", Typ-ID: " << static_cast<int>(netWp.getType()) << " -> Zone: At_Entry" << std::endl;
}

void WorkpieceManager::takt(bool isMotorSlow) {
    for (auto& wp : m_activeWorkpieces) {

        // ====================================================================
        // Stationäre Zustände hängen rein von Hardware-Flanken ab (Kein Altern!)
        // ====================================================================
        if (wp.getLocation() == WpLocation::AT_ENTRY ||
            wp.getLocation() == WpLocation::AT_HEIGHT_SENSOR ||
            wp.getLocation() == WpLocation::AT_EJECTOR ||
            wp.getLocation() == WpLocation::AT_CHUTE ||
            wp.getLocation() == WpLocation::AT_END) {
            continue;
        }

        // Werkstück befindet sich im Transit -> Altern um genau +1 Takt (alle 10ms)
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

                if (wp.getTicks() > wp.getTargetLimit() + 30) {
                    std::cerr << "[DIAGNOSE-ERROR] WP " << wp.getId() << " verloren zwischen Einlauf und Hoehenmessung!" << std::endl;
                    m_context->processEvent(logik::Event::ERROR);
                }
                break;

            case WpLocation::TRANSIT_TO_EJECTOR:
                {
                    std::uint32_t currentLimit = isMotorSlow ? m_calibData.ticks_HeightC_to_EjectorB_Slow
                                                             : m_calibData.ticks_HeightC_to_EjectorB_Normal;

                    if (wp.getTicks() > currentLimit + 30) {
                        std::cerr << "[DIAGNOSE-ERROR] WP " << wp.getId() << " verloren zwischen Hoehenmessung und Weiche!" << std::endl;
                        m_context->processEvent(logik::Event::ERROR);
                    }
                }
                break;

            case WpLocation::TRANSIT_TO_CHUTE:
                if (wp.getTicks() > wp.getTargetLimit()) {
                    std::cerr << "[DIAGNOSE-ERROR] WP " << wp.getId() << " in der Sortierrutsche stecken geblieben!" << std::endl;
                    m_context->processEvent(logik::Event::ERROR);
                }
                break;

            case WpLocation::TRANSIT_TO_END:
                // Wegstrecken-Watchdog für den Weg nach der Sortierweiche
                if (wp.getTicks() > wp.getTargetLimit() + 30) {
                    std::cerr << "[DIAGNOSE-ERROR] WP " << wp.getId() << " auf dem Weg zum Bandende verloren!" << std::endl;

                    // ========================================================
                    // ABSOLUTE REZEPT-SYNCHRONISATION BEI WATCHDOG:
                    // Wenn das Teil im Transit verloren geht, korrigieren wir uns
                    // lokal hart über forceType() mit dem Typ, den dieses WP hatte.
                    // ========================================================
                    if (wp.isCorrectInSequence()) {
                        m_sequence.forceType(wp.getType());
                    }

                    m_context->processEvent(logik::Event::ERROR);
                }
                break;

            default:
                break;
        }
    }
}

void WorkpieceManager::handleSensorSignal(logik::Event e, WpType typeFromHal) {

    switch (e) {
        // ====================================================================
        // 1. ZONE: EINLAUF
        // ====================================================================
        case logik::Event::ENTRY_BLOCKED: {
            std::cerr << "[CRITICAL ARCHITECTURE] ENTRY_BLOCKED darf nicht handleSensorSignal triggern! Nutzen Sie addWorkpiece()." << std::endl;
            m_context->processEvent(logik::Event::ERROR);
            break;
        }

        case logik::Event::ENTRY_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_ENTRY);
            if (wp != nullptr) {
                std::cout << "[HAL-Signal] ENTRY_CLEARED fuer WP " << wp->getId() << " -> Transit zum HS." << std::endl;
                wp->updateLocation(WpLocation::TRANSIT_TO_HEIGHT);
                wp->setTargetLimit(m_calibData.ticks_EntryC_to_HeightB_Normal);
            } else {
                std::cerr << "[CRITICAL SAFETY] ENTRY_CLEARED gefeuert, aber kein WP an Position At_Entry!" << std::endl;
                m_context->processEvent(logik::Event::ERROR);
            }
            break;
        }

        // ====================================================================
        // 2. ZONE: HÖHENMESSUNG & METALLERKENNUNG
        // ====================================================================
        case logik::Event::HEIGHT_BLOCKED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_HEIGHT);
            if (wp != nullptr) {
                std::cout << "[HAL-Signal] WP " << wp->getId() << " an Hoehenmessung eingetroffen." << std::endl;
                wp->updateLocation(WpLocation::AT_HEIGHT_SENSOR);
            } else {
                std::cerr << "[CRITICAL SAFETY] HEIGHT_BLOCKED gefeuert, aber kein WP im Transit!" << std::endl;
                m_context->processEvent(logik::Event::ERROR);
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
                std::cerr << "[CRITICAL SAFETY] HEIGHT_CLEARED gefeuert, aber kein WP beim Sensor registriert!" << std::endl;
                m_context->processEvent(logik::Event::ERROR);
            }
            break;
        }

        case logik::Event::HEIGHT_ERROR: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_HEIGHT_SENSOR);
            if (wp != nullptr) {
                std::cout << "[WARNUNG] HEIGHT_ERROR für WP " << wp->getId() << ". Markiere als UNKNOWN für Aussortierung." << std::endl;
                wp->setType(WpType::UNKNOWN);
            }
            break;
        }

        case logik::Event::METAL_DETECTED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR);

            if (wp != nullptr) {
                std::cout << "[HAL-Signal] METAL_DETECTED fuer WP " << wp->getId()
                          << " (Aktueller Typ vor Upgrade: " << static_cast<int>(wp->getType()) << ")" << std::endl;

                if (wp->getType() == WpType::TALL_DRILLED) {
                    wp->setType(WpType::TALL_DRILLED_METAL);
                }
                else if (wp->getType() == WpType::TALL) {
                    wp->setType(WpType::TALL_METAL);
                }

                std::cout << "[WP-Manager] Metall-Upgrade erfolgreich! Neuer Typ: "
                          << static_cast<int>(wp->getType()) << std::endl;
            }
            else {
                std::cerr << "[CRITICAL SAFETY] METAL_DETECTED gefeuert, aber kein WP im Zustand Transit_To_Ejector!" << std::endl;
                m_context->processEvent(logik::Event::ERROR);
            }
            break;
        }

        case logik::Event::METAL_CLEARED: {
            break;
        }

        // ====================================================================
        // 3. ZONE: SORTIERWEICHE (EJECTOR)
        // ====================================================================
        case logik::Event::EJECTOR_BLOCKED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_EJECTOR);

            if (wp != nullptr) {
                std::cout << "[HAL-Signal] WP " << wp->getId() << " erreicht physisch die Sortierweiche." << std::endl;
                wp->updateLocation(WpLocation::AT_EJECTOR);

                checkAndSetJunction(wp);
            } else {
                std::cerr << "[CRITICAL SAFETY] EJECTOR_BLOCKED gefeuert, aber kein WP im Anflug!" << std::endl;
                m_context->processEvent(logik::Event::ERROR);
            }
            break;
        }

        case logik::Event::EJECTOR_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_EJECTOR);

            if (wp != nullptr) {
                std::cout << "[HAL-Signal] WP " << wp->getId() << " hat Weichenbereich verlassen." << std::endl;

                if (wp->isCorrectInSequence()) {
                    // Gutes Teil -> Wechselt in Transit zum Bandende
                    std::cout << "[WP-Manager] WP " << wp->getId() << " faehrt weiter geradeaus zum Bandende." << std::endl;
                    wp->updateLocation(WpLocation::TRANSIT_TO_END);
                    wp->setTargetLimit(m_calibData.ticks_EjectorC_to_EndB_Normal);

                    // Rezeptur rückt sofort nach sicherer Passage der Weiche vor
                    m_sequence.advanceSequence();
                }
                else {
                    // Schlechtes Teil -> Ist in die Sortierrutsche abgebogen
                    std::cout << "[WP-Manager] WP " << wp->getId() << " erfolgreich in Richtung Rutsche abgebogen." << std::endl;
                    wp->updateLocation(WpLocation::TRANSIT_TO_CHUTE);
                    wp->setTargetLimit(150); // 1.5 Sekunden Rutschen-Watchdog starten
                }
            } else {
                std::cerr << "[CRITICAL SAFETY] EJECTOR_CLEARED gefeuert, aber kein WP an der Weiche registriert!" << std::endl;
                m_context->processEvent(logik::Event::ERROR);
            }
            break;
        }

        // ====================================================================
        // 4. ZONE: BANDENDE
        // ====================================================================
        case logik::Event::END_BLOCKED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_END);
            if (wp != nullptr) {
                std::cout << "[HAL-Signal] WP " << wp->getId() << " am Bandende angekommen." << std::endl;
                wp->updateLocation(WpLocation::AT_END);

                // Übergabe-Absicherung für gekoppelte Anlagen
                if (m_myPosition == AnlagenPosition::VORN_ANLAGE_1 && !m_isAnlage2Ready) {
                    std::cerr << "[CRITICAL SAFETY] WP wartet am Uebergabepunkt, aber Anlage 2 ist NICHT BEREIT!" << std::endl;

                    // Fehler im Übergabeprozess: Da das Band stoppt, zwingen wir das Rezept
                    // lokal hart wieder auf den Typen dieses blockierten Werkstücks.
                    if (wp->isCorrectInSequence()) {
                        m_sequence.forceType(wp->getType());
                    }
                    m_context->processEvent(logik::Event::SET_STOP); // Motor-Stopp anfordern
                }
            } else {
                std::cerr << "[CRITICAL SAFETY] END_BLOCKED gefeuert, aber kein WP im End-Transit!" << std::endl;
                m_context->processEvent(logik::Event::ERROR);
            }
            break;
        }

        case logik::Event::END_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_END);
            if (wp != nullptr) {

                if (m_myPosition == AnlagenPosition::VORN_ANLAGE_1) {
                    if (m_isAnlage2Ready) {
                        std::cout << "[HAL-Signal] WP " << wp->getId() << " an Anlage 2 uebergeben." << std::endl;
                        m_context->getDispatcher()->workpieceInfo(wp->getId(), wp->getType());
                    } else {
                        std::cerr << "[CRITICAL SAFETY] END_CLEARED gefeuert, obwohl Anlage 2 blockiert war!" << std::endl;

                        if (wp->isCorrectInSequence()) {
                            m_sequence.forceType(wp->getType());
                        }
                        m_context->processEvent(logik::Event::ERROR);
                        break;
                    }
                } else {
                    std::cout << "[HAL-Signal] WP " << wp->getId() << " hat das Gesamtsystem erfolgreich verlassen." << std::endl;
                }

                removeWorkpiece(wp->getId());
            } else {
                std::cerr << "[CRITICAL SAFETY] END_CLEARED gefeuert, aber kein WP am Bandende!" << std::endl;
                m_context->processEvent(logik::Event::ERROR);
            }
            break;
        }

        // ====================================================================
        // 5. ZONE: SORTIERRUTSCHE (CHUTE)
        // ====================================================================
        case logik::Event::CHUTE_BLOCKED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::TRANSIT_TO_CHUTE);
            if (wp != nullptr) {
                std::cout << "[HAL-Signal] WP " << wp->getId() << " erfolgreich in der Rutsche gelandet." << std::endl;
                wp->updateLocation(WpLocation::AT_CHUTE);
            } else {
                std::cerr << "[CRITICAL SAFETY] CHUTE_BLOCKED gefeuert, aber kein WP im Rutschen-Transit!" << std::endl;
                m_context->processEvent(logik::Event::ERROR);
            }
            break;
        }

        case logik::Event::CHUTE_CLEARED: {
            Workpiece* wp = getFirstWorkpieceAt(WpLocation::AT_CHUTE);
            if (wp != nullptr) {
                std::cout << "[HAL-Signal] WP " << wp->getId() << " aus der Rutsche entnommen. Loesche Zwilling." << std::endl;
                removeWorkpiece(wp->getId());
            } else {
                std::cerr << "[CRITICAL SAFETY] CHUTE_CLEARED gefeuert, aber kein WP in der Rutsche!" << std::endl;
                m_context->processEvent(logik::Event::ERROR);
            }
            break;
        }

        default:
            break;
    }
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
}

void WorkpieceManager::clearAll() {
    m_activeWorkpieces.clear();
    m_sequence.resetSequence();
    std::cout << "[WP-Manager] Schieberegister und Rezeptur komplett zurueckgesetzt." << std::endl;
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

        std::cout << "[WP-Manager] Anlage 2 Weichen-Check fuer WP " << wp->getId() << std::endl;
        std::cout << "  - Netzwerk-Rezept-Flag war: " << (networkSaysOk ? "OK" : "FALSCH") << std::endl;
        std::cout << "  - Reale Messung auf Band 2 ist: " << (hardwareMeasuresOk ? "OK" : "WIDERSPRUCH") << std::endl;

        // ====================================================================
        // ABSOLUTE REZEPT-SYNCHRONISATION VIA updateWpListe
        // Wenn ein Widerspruch auftritt, senden wir den erwarteten Typ
        // per Netzwerkbefehl an Anlage 1. Diese erzwingt dort per forceType()
        // das korrekte Rezept.
        // ====================================================================
        if (networkSaysOk && !hardwareMeasuresOk) {
            WpType missingType = m_sequence.getExpectedType();

            std::cerr << "[CRITICAL CONFLICT] WP " << wp->getId()
                      << " digital korrekt, aber physisch manipuliert!" << std::endl;
            std::cerr << " -> Sende absoluten Rezept-Typ (" << static_cast<int>(missingType)
                      << ") via updateWpListe an Anlage 1." << std::endl;

            m_context->getDispatcher()->updateWpListe(missingType);
            m_context->processEvent(logik::Event::ERROR);
        }

        std::cout << "  -> Gesamt-Entscheidung: " << (correct ? "PASSIEREN" : "AUSSORTIEREN") << std::endl;
    }

    wp->setIsCorrectInSequence(correct);

    if (correct) {
        std::cout << "[WP-Manager] WP " << wp->getId() << " ist gueltig. Weiche bleibt zu." << std::endl;
        m_context->getDispatcher()->closeJunction();
    } else {
        std::cout << "[WP-Manager] WP " << wp->getId() << " ungueltig oder manipuliert! Oeffne Weiche." << std::endl;
        m_context->getDispatcher()->openJunction();
    }
}

} // namespace logik
