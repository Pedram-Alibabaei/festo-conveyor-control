/**
 * @file Timer.cpp
 * @brief Implementierung der QNX POSIX-Timer.
 *
 * SoSe2026 Team 3.3
 */

#include "timer.hpp"
#include "shared/ipc_definitions.hpp" // Sehr wichtig fuer die Puls-Codes!
#include "shared/detailLog.hpp"
#include <iostream>

Timer::Timer() : m_localCoid(-1),
                 m_beltTimerId(-1),
                 m_heartbeatTimerId(-1) {
}

Timer::~Timer() {
    stop();
    // Verbleibende Timer-Ressourcen im Betriebssystem sauber loeschen
    if (m_beltTimerId != -1) {
        timer_delete(m_beltTimerId);
    }
    if (m_heartbeatTimerId != -1) {
        timer_delete(m_heartbeatTimerId);
    }
}

void Timer::init(int localCoid) {
    m_localCoid = localCoid;

    // 1. Belt-Timer beim OS registrieren
    createQnxTimer(m_beltTimerId, ipc::BELT_TICK, 0);

    // 2. Heartbeat-Timer beim OS registrieren
    createQnxTimer(m_heartbeatTimerId, ipc::HEARTBEATTACKT, 0);

    DETAIL_LOG( "[Timer] QNX-Timer (Belt & Heartbeat) erfolgreich initialisiert.");
}

// ============================================================================
// BELT-TIMER (Zyklischer Takt fuer das Schieberegister)
// ============================================================================
void Timer::startBeltTimer(int ms) {
    DETAIL_LOG( "[Timer] Starte Belt-Timer mit " << ms << "ms Intervall.");
    setQnxTimerInterval(m_beltTimerId, ms);
}

void Timer::stopBeltTimer() {
    setQnxTimerInterval(m_beltTimerId, 0); // 0ms bedeutet "Stopp" in QNX
}

// ============================================================================
// HEARTBEAT-TIMER (Zyklischer Takt fuer die Watchdog-Verbindung)
// ============================================================================
void Timer::startHeartbeatTimer(int ms) {
    DETAIL_LOG( "[Timer] Starte Heartbeat-Timer mit " << ms << "ms Intervall.");
    setQnxTimerInterval(m_heartbeatTimerId, ms);
}

void Timer::stopHeartbeatTimer() {
    setQnxTimerInterval(m_heartbeatTimerId, 0);
}

// ============================================================================
// STOP ALL
// ============================================================================
void Timer::stop() {
    stopBeltTimer();
    stopHeartbeatTimer();
}

// ============================================================================
// PRIVATE HILFSMETHODEN (QNX SYSTEM-CALLS)
// ============================================================================

void Timer::createQnxTimer(timer_t& timerId, int pulseCode, int pulseValue) {
    if (m_localCoid == -1) return;

    struct sigevent event;
    // Wenn die Uhr ablaeuft, wird ein OS-Puls mit dem spezifischen Code an den Dispatcher gefeuert
    SIGEV_PULSE_INIT(&event, m_localCoid, SIGEV_PULSE_PRIO_INHERIT, pulseCode, pulseValue);

    // Uhr beim Betriebssystem registrieren
    if (timer_create(CLOCK_REALTIME, &event, &timerId) == -1) {
        std::cerr << "[Timer] Fehler beim Erstellen des Timers fuer Pulse: " << pulseCode << std::endl;
    }
}

void Timer::setQnxTimerInterval(timer_t timerId, int ms) {
    if (timerId == -1) return;

    struct itimerspec itime;

    // Wenn ms == 0 ist, wird der Timer gestoppt
    itime.it_value.tv_sec = ms / 1000;
    itime.it_value.tv_nsec = (ms % 1000) * 1000000;

    // Wiederholungsintervall setzen (macht ihn zum zyklischen Timer statt One-Shot)
    itime.it_interval.tv_sec = ms / 1000;
    itime.it_interval.tv_nsec = (ms % 1000) * 1000000;

    timer_settime(timerId, 0, &itime, NULL);
}