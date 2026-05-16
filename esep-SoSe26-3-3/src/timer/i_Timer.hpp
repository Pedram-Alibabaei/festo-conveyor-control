/**
* @file i_Timer.hpp
 * @brief Abstrakte Schnittstelle fuer die virtuellen Uhren (Belt & Heartbeat).
 */

#ifndef I_TIMER_HPP
#define I_TIMER_HPP

class i_Timer {
public:
 virtual ~i_Timer() = default;

 /**
  * @brief Initialisiert die Timer mit der lokalen COID des Dispatchers.
  */
 virtual void init(int localCoid) = 0;

 // ========================================================================
 // Belt-Uhr (Fuer die Werkstueck-Verfolgung)
 // ========================================================================
 virtual void startBeltTimer(int ms) = 0;
 virtual void stopBeltTimer() = 0;

 // ========================================================================
 // Heartbeat-Uhr (Fuer die Netzwerk-Ueberwachung)
 // ========================================================================
 virtual void startHeartbeatTimer(int ms) = 0;
 virtual void stopHeartbeatTimer() = 0;

 // ========================================================================
 //
 // ========================================================================
 virtual void startSorterTimer(int ms) = 0;
 virtual void stopSorterTimer() = 0;
 /**
  * @brief Stoppt alle laufenden Timer sofort.
  */
 virtual void stop() = 0;
};

#endif // I_TIMER_HPP
