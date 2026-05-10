/**
 * @file BeltTimer.cpp
 */

#include "BeltTimer.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>

BeltTimer::BeltTimer(int coid, int pulseCode) : m_coid(coid), m_pulseCode(pulseCode) {

    // 1. Das Event vorbereiten: "Sende einen Puls an 'coid' mit dem Code 'pulseCode'"
    // SIGEV_PULSE_PRIO_INHERIT bedeutet, der Puls bekommt die normale Prioritaet.
    SIGEV_PULSE_INIT(&m_event, m_coid, SIGEV_PULSE_PRIO_INHERIT, m_pulseCode, 0);

    // 2. Den Timer beim Betriebssystem anmelden (aber noch nicht starten)
    // CLOCK_REALTIME ist die Standard-Systemuhr.
    if (timer_create(CLOCK_REALTIME, &m_event, &m_timerId) == -1) {
        std::cerr << "[BeltTimer] FEHLER: Konnte Timer nicht erstellen! "
                  << strerror(errno) << std::endl;
    } else {
        std::cout << "[BeltTimer] Uhr erfolgreich registriert (ID: " << m_timerId << ")." << std::endl;
    }
}

BeltTimer::~BeltTimer() {
    stop(); // Sicherstellen, dass er nicht weiterfeuert
    timer_delete(m_timerId); // Den Wecker beim OS abmelden
    std::cout << "[BeltTimer] Uhr zerstoert." << std::endl;
}

void BeltTimer::startPeriodic(int ms) {
    struct itimerspec itime;

    // Zeitberechnung: QNX braucht Sekunden (sec) und Nanosekunden (nsec)
    // 1 Millisekunde = 1.000.000 Nanosekunden

    // it_value bestimmt, wann der Timer das ERSTE Mal feuert
    itime.it_value.tv_sec = ms / 1000;
    itime.it_value.tv_nsec = (ms % 1000) * 1000000;

    // it_interval bestimmt, in welchem Abstand er DANACH immer wieder feuert
    itime.it_interval.tv_sec = ms / 1000;
    itime.it_interval.tv_nsec = (ms % 1000) * 1000000;

    // Timer scharfschalten!
    if (timer_settime(m_timerId, 0, &itime, NULL) == -1) {
        std::cerr << "[BeltTimer] FEHLER beim Starten des Timers: "
                  << strerror(errno) << std::endl;
    } else {
        std::cout << "[BeltTimer] Uhr gestartet: " << ms << "ms Takt." << std::endl;
    }
}

void BeltTimer::stop() {
    struct itimerspec itime;

    // Wenn wir alle Werte auf 0 setzen, stoppt QNX den Timer sofort
    itime.it_value.tv_sec = 0;
    itime.it_value.tv_nsec = 0;
    itime.it_interval.tv_sec = 0;
    itime.it_interval.tv_nsec = 0;

    timer_settime(m_timerId, 0, &itime, NULL);
    std::cout << "[BeltTimer] Uhr gestoppt." << std::endl;
}