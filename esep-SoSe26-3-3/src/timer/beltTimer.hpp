/**
* @file BeltTimer.hpp
 * @brief Konkrete Implementierung des Timers fuer QNX.
 */

#ifndef BELTTIMER_HPP
#define BELTTIMER_HPP

#include "I_Timer.hpp"
#include <time.h>
#include <sys/siginfo.h>

class BeltTimer : public i_Timer {
private:
    timer_t m_timerId;        // Die ID des Timers vom Betriebssystem
    struct sigevent m_event;  // Beschreibt, WAS passieren soll, wenn der Timer ablaeuft

    int m_coid;               // Die Verbindung zum Dispatcher
    int m_pulseCode;          // Der Code (z.B. ipc::TIMER_TICK), der gesendet werden soll

public:
    /**
     * @brief Konstruktor
     * @param coid Connection-ID zum Dispatcher, damit der Timer weiss, wohin der Puls muss.
     * @param pulseCode Der Code, an dem der Dispatcher erkennt, dass es die Uhr ist.
     */
    BeltTimer(int coid, int pulseCode);

    virtual ~BeltTimer();

    void startPeriodic(int ms) override;
    void stop() override;
};

#endif // BELTTIMER_HPP