/**
 * @file motor_controller.cpp
 * @brief Implementierung der sicheren Motorsteuerung.
 */

#include "motor_controller.hpp"

#include <iostream>

// Eure echte PAL einbinden
#include <hal/pal.hpp>

namespace hal {
namespace output_driver {

MotorController::MotorController() : m_currentState(MotorState::STOPPED) {
    // 1. Pins als Ausgänge konfigurieren (Sehr wichtig, sonst ignoriert die Hardware unsere Befehle!)
    pal::op::Forward::config_as_output();
    pal::op::Reverse::config_as_output();
    pal::op::Slow::config_as_output();
    pal::op::Stop::config_as_output();

    // 2. Beim Hochfahren sicherstellen, dass der Motor wirklich steht
    stop();
}

MotorController::~MotorController() {
    // Wenn das System beendet wird, schalten wir den Motor sicherheitshalber ab.
    stop();
}

void MotorController::resetAllPins() {
    // Schaltet alle Motor-Richtungen und Geschwindigkeiten hardwareseitig ab
    pal::op::Forward::data_clear();
    pal::op::Reverse::data_clear();
    pal::op::Slow::data_clear();
    pal::op::Stop::data_clear();
}

void MotorController::stop() {
    if (m_currentState == MotorState::STOPPED) {
        return; // Nichts tun, wir stehen schon!
    }

    resetAllPins();

    // Festo-Spezialität: Der Motor hat einen eigenen "Stop"-Pin für aktives Bremsen
    pal::op::Stop::data_set();

    m_currentState = MotorState::STOPPED;
    // std::cout << "[MotorController] Motor GESTOPPT." << std::endl; // Auskommentiert für weniger Konsolen-Spam
}

void MotorController::driveRight() {
    if (m_currentState == MotorState::RIGHT) {
        return;
    }

    // 1. SICHERHEITS-INTERLOCK: Erst alles abschalten (auch die Bremse lösen)
    resetAllPins();

    // 2. Rechtslauf aktivieren
    pal::op::Forward::data_set();

    m_currentState = MotorState::RIGHT;
    // std::cout << "[MotorController] Motor faehrt RECHTS." << std::endl;
}

void MotorController::driveLeft() {
    if (m_currentState == MotorState::LEFT) {
        return;
    }

    resetAllPins();

    // Linkslauf aktivieren
    pal::op::Reverse::data_set();

    m_currentState = MotorState::LEFT;
    // std::cout << "[MotorController] Motor faehrt LINKS." << std::endl;
}

void MotorController::driveSlowRight() {
    if (m_currentState == MotorState::SLOW_RIGHT) {
        return;
    }

    resetAllPins();

    // Rechtslauf + Langsam-Pin setzen
    pal::op::Forward::data_set();
    pal::op::Slow::data_set();

    m_currentState = MotorState::SLOW_RIGHT;
    // std::cout << "[MotorController] Motor faehrt LANGSAM RECHTS." << std::endl;
}

void MotorController::driveSlowLeft() {
    if (m_currentState == MotorState::SLOW_LEFT) {
        return;
    }

    resetAllPins();

    // Linkslauf + Langsam-Pin setzen
    pal::op::Reverse::data_set();
    pal::op::Slow::data_set();

    m_currentState = MotorState::SLOW_LEFT;
    // std::cout << "[MotorController] Motor faehrt LANGSAM LINKS." << std::endl;
}

} // namespace output_driver
} // namespace hal