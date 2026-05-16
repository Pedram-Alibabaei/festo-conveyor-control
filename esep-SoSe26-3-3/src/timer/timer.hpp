/**
 * @file Timer.hpp
 * @brief Implementierung der QNX POSIX-Timer.
 */

#ifndef TIMER_HPP
#define TIMER_HPP

#include "i_Timer.hpp"
#include <time.h>
#include <sys/siginfo.h>

class Timer : public i_Timer {
private:
    int m_localCoid;

    timer_t m_beltTimerId;      // QNX ID fuer den Band-Timer
    timer_t m_heartbeatTimerId; // QNX ID fuer den Heartbeat-Timer
    timer_t m_sorterTimerId;    // QNX ID fuer den Weichen/Auswerfer-Timer

    // Hilfsmethoden, um Code-Duplizierung zu vermeiden
    void createQnxTimer(timer_t& timerId, int pulseCode, int pulseValue);
    void setQnxTimerInterval(timer_t timerId, int ms);

public:
    Timer();
    ~Timer() override;

    void init(int localCoid) override;

    void startBeltTimer(int ms) override;
    void stopBeltTimer() override;

    void startHeartbeatTimer(int ms) override;
    void stopHeartbeatTimer() override;

    void startSorterTimer(int ms) override;
    void stopSorterTimer() override;

    void stop() override;
};

#endif // TIMER_HPP
