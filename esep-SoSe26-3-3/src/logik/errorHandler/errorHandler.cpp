/**
 * @file errorHandler.cpp
 * @brief Implementierung des zentralen Fehler- und Warnungsverwalters.
 */

#include "errorHandler.hpp"
#include <iostream>

namespace logik {

// ============================================================================
// KONSTRUKTOR
// ============================================================================
ErrorHandler::ErrorHandler()
    : m_currentError(logik::ErrorID::NONE),
      m_currentWarning(logik::WarningID::NONE),
      m_localChuteFull(false),
      m_partnerChuteFull(false) {
}

// ============================================================================
// SENSOR-UPDATES (ZUSTANDSMELDUNGEN)
// ============================================================================

void ErrorHandler::setLocalChuteStatus(bool isFull) {
    if (m_localChuteFull != isFull) {
        m_localChuteFull = isFull;
        evaluateChuteStates();
    }
}

void ErrorHandler::setPartnerChuteStatus(bool isFull) {
    if (m_partnerChuteFull != isFull) {
        m_partnerChuteFull = isFull;
        evaluateChuteStates();
    }
}

void ErrorHandler::triggerManualError(logik::ErrorID error) {
    m_currentError = error;
    // Wenn ein kritischer Fehler manuell gesetzt wird, überschreibt das temporär Warnungen
    if (error != logik::ErrorID::NONE) {
        m_currentWarning = logik::WarningID::NONE;
    }
}

// ============================================================================
// DAS DIAGNOSE-GEHIRN (DIREKT NACH DEINER SPEZIFIKATION)
// ============================================================================

void ErrorHandler::evaluateChuteStates() {
    // Falls ein anderer kritischer Fehler (wie Verbindungsabbruch oder WP_LOST)
    // manuell gesetzt wurde, lassen wir diesen unangetastet, es sei denn, wir berechnen Rutschenfehler.
    if (m_currentError != logik::ErrorID::NONE && m_currentError != logik::ErrorID::BOTH_CHUTES_FULL) {
        return;
    }

    if (m_localChuteFull && m_partnerChuteFull) {
        // Kritischer Fall: Beide voll -> Bandstopp zwingend erforderlich!
        m_currentError = logik::ErrorID::BOTH_CHUTES_FULL;
        m_currentWarning = logik::WarningID::NONE;
    }
    else if (m_localChuteFull) {
        // Nur die eigene voll -> Warnung (Band läuft weiter, Ausweichbetrieb)
        m_currentError = logik::ErrorID::NONE;
        m_currentWarning = logik::WarningID::LOCAL_CHUTE_FULL;
    }
    else if (m_partnerChuteFull) {
        // Nur die beim Partner voll -> Warnung (Band läuft weiter, rein lokales Sortieren)
        m_currentError = logik::ErrorID::NONE;
        m_currentWarning = logik::WarningID::PARTNER_CHUTE_FULL;
    }
    else {
        // Beide Rutschen sind leer -> Alles im grünen Bereich
        m_currentError = logik::ErrorID::NONE;
        m_currentWarning = logik::WarningID::NONE;
    }
}

// ============================================================================
// SYSTEM-RESET & GETTER
// ============================================================================

void ErrorHandler::clearAll() {
    std::cout << "[ErrorHandler] Reset-Befehl ausgefuehrt. Berechne Sensorstatus neu..." << std::endl;
    m_currentError = logik::ErrorID::NONE;
    m_currentWarning = logik::WarningID::NONE;

    // Nach dem Löschen direkt prüfen, ob die Lichtschranken immer noch blockiert sind!
    evaluateChuteStates();
}

logik::ErrorID ErrorHandler::getError() const {
    return m_currentError;
}

logik::WarningID ErrorHandler::getWarning() const {
    return m_currentWarning;
}

bool ErrorHandler::hasCriticalError() const {
    return m_currentError != logik::ErrorID::NONE;
}

bool ErrorHandler::hasWarning() const {
    return m_currentWarning != logik::WarningID::NONE;
}

bool ErrorHandler::isSystemClear() const {
    // Das System gilt nur dann als komplett fehlerfrei und bereit,
    // wenn keine hardwareseitige Blockade der Rutschen mehr vorliegt
    // und kein kritischer Folgefehler mehr aktiv ist.
    return (!m_localChuteFull && !m_partnerChuteFull && m_currentError == logik::ErrorID::NONE);
}

} // namespace logik