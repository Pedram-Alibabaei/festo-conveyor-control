/**
* @file I_Timer.hpp
 * @brief Abstrakte Schnittstelle fuer die virtuelle Band-Uhr.
 */

#ifndef I_TIMER_HPP
#define I_TIMER_HPP

class i_Timer {
public:
    virtual ~i_Timer() = default;

    /**
     * @brief Startet den periodischen Takt.
     * @param ms Intervall in Millisekunden (z.B. 50 für alle 50ms).
     */
    virtual void startPeriodic(int ms) = 0;

    /**
     * @brief Stoppt die Band-Uhr.
     */
    virtual void stop() = 0;
};

#endif // I_TIMER_HPP