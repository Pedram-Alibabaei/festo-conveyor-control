/**
 * @file Timer.cpp
 * @brief Implementierung der QNX POSIX-Timer.
 */

#include "timer.hpp"
#include "shared/ipc_definitions.hpp" // Sehr wichtig fuer die Puls-Codes!
#include <iostream>

Timer::Timer() : m_localCoid(-1),
                 m_beltTimerId(-1),
                 m_heartbeatTimerId(-1),
                 m_sorterTimerId(-1){
}

Timer::~Timer() {
    stop();
    // Timer-Ressourcen im Betriebssystem sauber loeschen
    if (m_beltTimerId != -1) timer_delete(m_beltTimerId);
    if (m_heartbeatTimerId != -1) timer_delete(m_heartbeatTimerId);
    if (m_sorterTimerId != -1) timer_delete(m_sorterTimerId);
}

void Timer::init(int localCoid) {
    m_localCoid = localCoid;

    // 1. Belt-Timer erstellen
    // ACHTUNG: Hier nutzen wir jetzt den neuen Enum-Namen BELT_TICK!
    createQnxTimer(m_beltTimerId, ipc::BELT_TICK, 0);

    // 2. Heartbeat-Timer erstellen
    createQnxTimer(m_heartbeatTimerId, ipc::HEARTBEATTACKT, 0);

    // 3. Sorter-Timer erstellen
    createQnxTimer(m_sorterTimerId, ipc::SORTER_TIMER_TICK, 0);

    std::cout << "[Timer] QNX-Timer erfolgreich initialisiert." << std::endl;
}

// ============================================================================
// BELT-TIMER
// ============================================================================
void Timer::startBeltTimer(int ms) {
    std::cout << "[Timer] Starte Belt-Timer mit " << ms << "ms Intervall." << std::endl;
    setQnxTimerInterval(m_beltTimerId, ms);
}

void Timer::stopBeltTimer() {
    setQnxTimerInterval(m_beltTimerId, 0); // 0ms bedeutet "Stopp" in QNX
}

// ============================================================================
// HEARTBEAT-TIMER
// ============================================================================
void Timer::startHeartbeatTimer(int ms) {
    std::cout << "[Timer] Starte Heartbeat-Timer mit " << ms << "ms Intervall." << std::endl;
    setQnxTimerInterval(m_heartbeatTimerId, ms);
}

void Timer::stopHeartbeatTimer() {
    setQnxTimerInterval(m_heartbeatTimerId, 0);
}

// ============================================================================
// SORTER-TIMER (One-Shot)
// ============================================================================
void Timer::startSorterTimer(int ms) {
    if (m_sorterTimerId == -1) return;

    struct itimerspec itime;

    // Wann soll er feuern?
    itime.it_value.tv_sec = ms / 1000;
    itime.it_value.tv_nsec = (ms % 1000) * 1000000;

    // WICHTIG: Interval auf 0! Das macht ihn zum One-Shot-Timer!
    itime.it_interval.tv_sec = 0;
    itime.it_interval.tv_nsec = 0;

    timer_settime(m_sorterTimerId, 0, &itime, NULL);
    std::cout << "[Timer] Sorter-Timer gestartet (" << ms << "ms)." << std::endl;
}

void Timer::stopSorterTimer() {
    if (m_sorterTimerId == -1) return;

    struct itimerspec itime = {0}; // Alles auf 0 setzt den Timer zurueck
    timer_settime(m_sorterTimerId, 0, &itime, NULL);
    std::cout << "[Timer] Sorter-Timer gestoppt/abgebrochen." << std::endl;
}

// ============================================================================
// STOP ALL
// ============================================================================
void Timer::stop() {
    stopBeltTimer();
    stopHeartbeatTimer();
    stopSorterTimer();
}

// ============================================================================
// PRIVATE HILFSMETHODEN (QNX SYSTEM-CALLS)
// ============================================================================

void Timer::createQnxTimer(timer_t& timerId, int pulseCode, int pulseValue) {
    if (m_localCoid == -1) return;

    struct sigevent event;
    // Wir definieren: Wenn die Uhr ablaeuft, feuere einen Puls an den Dispatcher!
    SIGEV_PULSE_INIT(&event, m_localCoid, SIGEV_PULSE_PRIO_INHERIT, pulseCode, pulseValue);

    // Uhr beim Betriebssystem registrieren
    if (timer_create(CLOCK_REALTIME, &event, &timerId) == -1) {
        std::cerr << "[Timer] Fehler beim Erstellen des Timers!" << std::endl;
    }
}

void Timer::setQnxTimerInterval(timer_t timerId, int ms) {
    if (timerId == -1) return;

    struct itimerspec itime;

    // Wenn ms == 0 ist, wird der Timer gestoppt
    // Ansonsten rechnen wir ms in Sekunden und Nanosekunden um
    itime.it_value.tv_sec = ms / 1000;
    itime.it_value.tv_nsec = (ms % 1000) * 1000000;

    // Wir wollen, dass er sich staendig wiederholt, also setzen wir auch das Intervall
    itime.it_interval.tv_sec = ms / 1000;
    itime.it_interval.tv_nsec = (ms % 1000) * 1000000;

    timer_settime(timerId, 0, &itime, NULL);
}
