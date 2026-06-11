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
      m_isSortingOut(false),
      m_keepRunning(true),
      m_isTimerActive(false),
      AUTOCLOSE_TIMEOUT(
#if defined(EJECTOR_TEST_FAST_TIMEOUT_MS)
          EJECTOR_TEST_FAST_TIMEOUT_MS
#else
          kAutoCloseSafetyTimeout
#endif
      )
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
        // Auswerfer: LOW = eingefahren (Ruhe), HIGH = ausgefahren
        pal::op::Ejector::data_clear();
    }
    m_isSortingOut.store(false);

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
        pal::op::Ejector::data_clear();
    }
}

bool EjectorController::isSwitch() const {
    return (m_type == SorterHardwareType::WEICHE);
}

void EjectorController::openSwitch() {
    if (m_type == SorterHardwareType::WEICHE && !m_isSortingOut.load()) {
        pal::op::Ejector::data_set();
        m_isSortingOut.store(true);

        // Timer aktivieren und Thread anstoßen
        m_startTime = std::chrono::steady_clock::now();
        m_isTimerActive.store(true);
        m_cv.notify_one();
    }
}

void EjectorController::closeSwitch() {
    if (m_type == SorterHardwareType::WEICHE && m_isSortingOut.load()) {
        pal::op::Ejector::data_clear();
        m_isSortingOut.store(false);
        m_isTimerActive.store(false);
    }
}

void EjectorController::pushEjector() {
    if (m_type == SorterHardwareType::AUSWERFER && !m_isSortingOut.load()) {
        pal::op::Ejector::data_set();
        m_isSortingOut.store(true);

        // Timer aktivieren und Thread anstoßen
        m_startTime = std::chrono::steady_clock::now();
        m_isTimerActive.store(true);
        m_cv.notify_one();
    }
}

void EjectorController::pullEjector() {
    if (m_type == SorterHardwareType::AUSWERFER && m_isSortingOut.load()) {
        pal::op::Ejector::data_clear();
        m_isSortingOut.store(false);
        m_isTimerActive.store(false);
    }
}

// ============================================================================
// DER AUTONOME BACKGROUND-WATCHDOG-THREAD
// ============================================================================
void EjectorController::watchdogWorker() {
    while (m_keepRunning.load()) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] {
            return m_isTimerActive.load() || !m_keepRunning.load();
        });

        if (!m_keepRunning.load()) {
            break;
        }

        while (m_isTimerActive.load() && m_keepRunning.load()) {
            const auto deadline = m_startTime + AUTOCLOSE_TIMEOUT;
            const bool deactivated = m_cv.wait_until(lock, deadline, [this] {
                return !m_isTimerActive.load() || !m_keepRunning.load();
            });

            if (deactivated || !m_keepRunning.load()) {
                break;
            }

            lock.unlock();
            if (m_type == SorterHardwareType::WEICHE) {
                closeSwitch();
            } else {
                pullEjector();
            }
            lock.lock();
        }
    }
}

} // namespace output_driver
} // namespace hal