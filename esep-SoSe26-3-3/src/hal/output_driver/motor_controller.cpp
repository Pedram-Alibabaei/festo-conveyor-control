/**
 * @file motor_controller.cpp
 * @brief Implementierung der sicheren Motorsteuerung.
 */

#include "motor_controller.hpp"
#include <iostream>
#include <hal/pal.hpp>

namespace hal {
namespace output_driver {

MotorController::MotorController() : m_currentState(MotorState::STOPPED) {
    // 1. Pins als Ausgänge konfigurieren
    pal::op::Forward::config_as_output();
    pal::op::Reverse::config_as_output();
    pal::op::Slow::config_as_output();
    pal::op::Stop::config_as_output();

    // 2. Beim Hochfahren den Motor sicher stoppen
    // Bei Festo bedeutet das oft: Richtungs-Pins auf 0, Stop-Pin auf 1 (Bremse aktiv)
    resetAllPins();
    pal::op::Stop::data_set();
}

MotorController::~MotorController() {
    stop();
}

void MotorController::resetAllPins() {
    // Schaltet alle Richtungen und Geschwindigkeiten ab
    pal::op::Forward::data_clear();
    pal::op::Reverse::data_clear();
    pal::op::Slow::data_clear();
    // Hinweis: Wir fassen den Stop-Pin hier nicht an,
    // das machen die Fahrfunktionen explizit selbst!
}

void MotorController::stop() {
    if (m_currentState == MotorState::STOPPED) {
        return;
    }

    resetAllPins();

    // Aktives Bremsen einschalten (Stop-Pin auf 1 setzen)
    pal::op::Stop::data_set();

    m_currentState = MotorState::STOPPED;
}

void MotorController::driveRight() {
    if (m_currentState == MotorState::RIGHT) {
        return;
    }

    // 1. Richtungs-Pins aufräumen
    resetAllPins();

    // 2. SICHERHEIT: Bremse lösen (Stop-Pin auf 0 loeschen), damit der Motor drehen kann!
    pal::op::Stop::data_clear();

    // 3. Rechtslauf aktivieren
    pal::op::Forward::data_set();

    m_currentState = MotorState::RIGHT;
}

void MotorController::driveLeft() {
    if (m_currentState == MotorState::LEFT) {
        return;
    }

    resetAllPins();
    pal::op::Stop::data_clear(); // Bremse lösen

    // Linkslauf aktivieren
    pal::op::Reverse::data_set();

    m_currentState = MotorState::LEFT;
}

void MotorController::driveSlowRight() {
    if (m_currentState == MotorState::SLOW_RIGHT) {
        return;
    }

    resetAllPins();
    pal::op::Stop::data_clear(); // Bremse lösen

    // Rechtslauf + Langsam-Pin setzen
    pal::op::Forward::data_set();
    pal::op::Slow::data_set();

    m_currentState = MotorState::SLOW_RIGHT;
}

void MotorController::driveSlowLeft() {
    if (m_currentState == MotorState::SLOW_LEFT) {
        return;
    }

    resetAllPins();
    pal::op::Stop::data_clear(); // Bremse lösen

    // Linkslauf + Langsam-Pin setzen
    pal::op::Reverse::data_set();
    pal::op::Slow::data_set();

    m_currentState = MotorState::SLOW_LEFT;
}

} // namespace output_driver
} // namespace hal