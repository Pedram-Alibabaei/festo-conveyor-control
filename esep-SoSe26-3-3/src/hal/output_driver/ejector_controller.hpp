/**
* @file ejector_controller.hpp
 * @brief Vollautonome Steuerung der Sortiereinheit mit internem Watchdog-Thread.
 *
 * SoSe2026 Team 3.3
 */

#ifndef HAL_OUTPUT_DRIVER_EJECTOR_CONTROLLER_HPP_
#define HAL_OUTPUT_DRIVER_EJECTOR_CONTROLLER_HPP_

#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace hal {
    namespace output_driver {

        enum class SorterHardwareType {
            WEICHE,
            AUSWERFER
        };

        /** Sicherheits-Auto-Close, falls Logik openSwitch/pushEjector nicht schliesst. */
        static constexpr std::chrono::milliseconds kAutoCloseSafetyTimeout{60000};

        class EjectorController {
        public:
            EjectorController();
            ~EjectorController();

            EjectorController(const EjectorController&) = delete;
            EjectorController& operator=(const EjectorController&) = delete;

            bool isSwitch() const;
            bool isActive() const { return m_isSortingOut.load(); }
            std::chrono::milliseconds getAutoCloseTimeout() const { return AUTOCLOSE_TIMEOUT; }

            // --- Weichen-Steuerung ---
            void openSwitch();
            void closeSwitch();

            /** Auswerfer-Impuls in Service-Kalibrierung (1 s). */
            static constexpr std::chrono::milliseconds kEjectorServicePushHold{1000};

            // --- Auswerfer-Steuerung ---
            void pushEjector();
            /** Push, danach automatisch pull nach holdTime (Watchdog-Thread). */
            void pushEjectorFor(std::chrono::milliseconds holdTime);
            void pullEjector();

        private:
            // Die interne Thread-Schleife
            void watchdogWorker();

            SorterHardwareType m_type;
            std::atomic<bool> m_isSortingOut;

            // --- Thread- und Synchronisations-Schnittstellen ---
            std::thread m_watchdogThread;
            std::atomic<bool> m_keepRunning;      ///< Steuert das Lebensende des Threads
            std::atomic<bool> m_isTimerActive;    ///< Zeigt an, ob die Weiche gerade offen ist

            std::mutex m_mutex;
            std::condition_variable m_cv;         ///< Zum effizienten Schlafen/Wecken des Threads

            std::chrono::steady_clock::time_point m_startTime;
            std::chrono::milliseconds m_activeHoldDuration{AUTOCLOSE_TIMEOUT};
            const std::chrono::milliseconds AUTOCLOSE_TIMEOUT;
        };

    } // namespace output_driver
} // namespace hal

#endif // HAL_OUTPUT_DRIVER_EJECTOR_CONTROLLER_HPP_