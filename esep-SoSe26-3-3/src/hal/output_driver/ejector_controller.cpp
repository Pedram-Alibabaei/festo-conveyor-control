/**
 * @file ejector_controller.cpp
 * @brief Implementierung des vollautonomen EjectorControllers mit internem Thread.
 *
 * SoSe2026 Team 3.3
 */

#include "ejector_controller.hpp"
#include <hal/pal.hpp>

namespace hal {
namespace output_driver {

EjectorController::EjectorController()
    : m_type(SorterHardwareType::WEICHE),
      m_isSortingOut(true),
      m_keepRunning(true),
      m_isTimerActive(false),
      AUTOCLOSE_TIMEOUT(60000) // 1 Minute Sicherheits-Timeout
{
    pal::is::Ejector::config_as_input();

    if (pal::is::Ejector::is_data_set()) {
        m_type = SorterHardwareType::AUSWERFER;
    } else {
        m_type = SorterHardwareType::WEICHE;
    }

    pal::op::Ejector::config_as_output();

    // Grundzustand herstellen
    if (m_type == SorterHardwareType::WEICHE) {
        pal::op::Ejector::data_clear();
    } else {
        pal::op::Ejector::data_set();
    }
    m_isSortingOut = false;

    // Start des autonomen Hintergrund-Watchdogs
    m_watchdogThread = std::thread(&EjectorController::watchdogWorker, this);
}

EjectorController::~EjectorController() {
    // Thread sauber beenden
    m_keepRunning = false;
    m_isTimerActive = false;
    m_cv.notify_all(); // Thread aus dem Schlaf wecken, damit er beenden kann

    if (m_watchdogThread.joinable()) {
        m_watchdogThread.join();
    }

    // Hardware in sicheren Zustand bringen
    if (m_type == SorterHardwareType::WEICHE) {
        pal::op::Ejector::data_clear();
    } else {
        pal::op::Ejector::data_set();
    }
}

bool EjectorController::isSwitch() const {
    return (m_type == SorterHardwareType::WEICHE);
}

void EjectorController::openSwitch() {
    if (m_type == SorterHardwareType::WEICHE && !m_isSortingOut) {
        pal::op::Ejector::data_set();
        m_isSortingOut = true;

        // Timer aktivieren und Thread anstoßen
        m_startTime = std::chrono::steady_clock::now();
        m_isTimerActive = true;
        m_cv.notify_one();
    }
}

void EjectorController::closeSwitch() {
    if (m_type == SorterHardwareType::WEICHE && m_isSortingOut) {
        pal::op::Ejector::data_clear();
        m_isSortingOut = false;
        m_isTimerActive = false; // Stoppt die Überwachung im Hintergrund
    }
}

void EjectorController::pushEjector() {
    if (m_type == SorterHardwareType::AUSWERFER && !m_isSortingOut) {
        pal::op::Ejector::data_clear();
        m_isSortingOut = true;

        // Timer aktivieren und Thread anstoßen
        m_startTime = std::chrono::steady_clock::now();
        m_isTimerActive = true;
        m_cv.notify_one();
    }
}

void EjectorController::pullEjector() {
    if (m_type == SorterHardwareType::AUSWERFER && m_isSortingOut) {
        pal::op::Ejector::data_set();
        m_isSortingOut = false;
        m_isTimerActive = false; // Stoppt die Überwachung im Hintergrund
    }
}

// ============================================================================
// DER AUTONOME BACKGROUND-WATCHDOG-THREAD
// ============================================================================
void EjectorController::watchdogWorker() {
    while (m_keepRunning) {
        std::unique_lock<std::mutex> lock(m_mutex);

        // Wenn der Timer NICHT aktiv ist, schläft der Thread hier stromsparend,
        // bis er über m_cv.notify_one() geweckt wird.
        m_cv.wait(lock, [this] { return m_isTimerActive || !m_keepRunning; });

        if (!m_keepRunning) break;

        // Ab hier ist die Weiche/der Auswerfer aktiv draußen.
        // Wir prüfen alle 20ms im Hintergrund die Zeit.
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        lock.lock();

        if (m_isTimerActive) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_startTime);

            if (duration >= AUTOCLOSE_TIMEOUT) {
                // Timeout erreicht! Da wir im Thread sind, rufen wir die
                // Schließfunktionen direkt auf.
                if (m_type == SorterHardwareType::WEICHE) {
                    pal::op::Ejector::data_clear();
                } else {
                    pal::op::Ejector::data_set();
                }
                m_isSortingOut = false;
                m_isTimerActive = false; // Geht im nächsten Schleifendurchlauf wieder schlafen
            }
        }
    }
}

} // namespace output_driver
} // namespace hal