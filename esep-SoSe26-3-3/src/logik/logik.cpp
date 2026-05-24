//
// Created by bahoz on 23.05.2026.
//

/**
 * @file Logik.cpp
 * @brief Implementierung des zentralen Logik-Herzens und Zustandsautomaten.
 */

#include "logik.hpp"
#include <iostream>
#include "shared/logikEvent.hpp"
#include "states/IdleState.hpp"
#include "states/BaseState.hpp"
// Falls Dispatcher-Methoden benötigt werden:
// #include "dispatcher/i_Dispatcher.hpp"

namespace logik {

Logik::Logik() : m_position(AnlagenPosition::VORN_ANLAGE_1), m_currentState(nullptr), m_dispatcher(nullptr), m_wpCount(0), m_nextWpId(1) {
    // Initialer Zustand
    changeState(new IdleState());
}

Logik::~Logik() {
    if (m_currentState) {
        delete m_currentState;
    }
}

void Logik::init(AnlagenPosition pos) {
    m_position = pos;
    std::string posName = (pos == AnlagenPosition::VORN_ANLAGE_1) ? "Anlage 1 (Vorn)" : "Anlage 2 (Hinten)";
    std::cout << "[Logik] Initialisiert fuer Position: " << posName << std::endl;
}

void Logik::processEvent(logik::Event e) {
    if (m_currentState) {
        m_currentState->handle(e);
    }
}

void Logik::workpieceTypeFromHS(WpType type) {
    std::cout << "[Logik-ADC] Profiler meldet erkannten Typ: " << static_cast<int>(type) << std::endl;
}

void Logik::workpieceInfo(int id, WpType type) {
    std::cout << "[Logik-Netzwerk] Info von Partner: ID " << id
              << " vom Typ " << static_cast<int>(type) << " kommt zu uns." << std::endl;
}

void Logik::RequestToGiveWp() {
    std::cout << "[Logik-Netzwerk] Anfrage: Wp abgeben." << std::endl;
}

void Logik::readyToReceiveWp() {
    std::cout << "[Logik-Netzwerk] Info: Bereit für Empfang." << std::endl;
}

void Logik::updateWpListe(WpType type) {
    std::cout << "[Logik-Netzwerk] Partner bittet um Listen-Update fuer Typ " << static_cast<int>(type) << std::endl;
}

void Logik::tackt() {
    // Diese Methode wird bei JEDEM Belt-Tick aufgerufen.
}

void Logik::setDispatcher(I_Dispatcher* dispatcher) {
    m_dispatcher = dispatcher;
}

I_Dispatcher* Logik::getDispatcher() const {
    return m_dispatcher;
}

void Logik::changeState(BaseState* newState) {
    if (m_currentState) {
        m_currentState->onExit();
        delete m_currentState;
    }
    m_currentState = newState;
    if (m_currentState) {
        m_currentState->setContext(this);
        m_currentState->onEnter();
    }
}

int Logik::getWorkpieceCount() const {
    return m_wpCount;
}

int Logik::getNextWpId() const {
    return m_nextWpId;
}

void Logik::addWorkpiece() {
    m_wpCount++;
    m_nextWpId++;
}

void Logik::removeWorkpiece() {
    if (m_wpCount > 0) m_wpCount--;
}

} // namespace logik
