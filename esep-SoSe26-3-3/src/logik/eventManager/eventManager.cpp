/**
 * @file eventManager.cpp
 * @brief Implementierung des EventManagers mit Einlauf-Timeout und Bandende-Schutz.
 */

#include "eventManager.hpp"
#include "logik/logik.hpp"
#include <iostream>

namespace logik {

// ============================================================================
// KONSTRUKTOR & INITIALISIERUNG
// ============================================================================
EventManager::EventManager(Logik* context)
    : m_context(context),
      m_isChuteTimerRunning(false),
      CHUTE_FILTER_TIME(2000),          // 2 Sekunden bis Rutsche VOLL
      m_isStartTimerRunning(false),
      START_LONG_THRESHOLD(3000),       // 3 Sekunden für langen Tastendruck
      m_isEntryTimerRunning(false),
      ENTRY_FILTER_TIME(2000),          // NEU: 2 Sekunden bis Einlauf-Fehler
      m_isEndBlocked(false)             // NEU: Standardmäßig frei
{
}

// ============================================================================
// VERARBEITUNG DER ROHEN EVENTS (VOM DISPATCHER)
// ============================================================================
void EventManager::handleRawEvent(logik::Event rawEvent) {
    if (!m_context) return;

    // ========================================================================
    // SONDERLOGIK: SPERRE BEI END_BLOCKED
    // ========================================================================
    // Wenn das Bandende blockiert ist, fangen wir kritische fahrrelevante Events ab
    // oder erzwingen Sicherheitsstopps.
    if (m_isEndBlocked) {
        // Erlaubt sind weiterhin Not-Halte, Resets, Cleared-Signale und Taster-Releases.
        // Wenn aber ein Event kommt, das den Motor starten will (wie ENTRY_BLOCKED), fangen wir es ab.
        if (rawEvent == logik::Event::ENTRY_BLOCKED) {
            std::cout << "[EventManager] SPERRE: ENTRY_BLOCKED ignoriert, da Bandende blockiert!" << std::endl;
            // Optional: Fehler an FSM senden, damit sie gar nicht erst anläuft
            m_context->processFilteredEvent(logik::Event::ERROR);
            return;
        }
    }

    switch (rawEvent) {

        // --- NEU: EINLAUF-LICHTSCHRANKE ÜBERWACHUNG ---
        case logik::Event::ENTRY_BLOCKED:
            if (!m_isEntryTimerRunning) {
                m_entryBlockedStartTime = std::chrono::steady_clock::now();
                m_isEntryTimerRunning = true;
                std::cout << "[EventManager] Einlauf belegt. Starte 2-Sekunden-Überwachung..." << std::endl;
            }
            // Wir leiten das rohe Event trotzdem an die FSM weiter, damit das Band normal anlaufen kann
            m_context->processFilteredEvent(rawEvent);
            break;

        case logik::Event::ENTRY_CLEARED:
            if (m_isEntryTimerRunning) {
                m_isEntryTimerRunning = false;
                std::cout << "[EventManager] Einlauf wieder frei. Zeitmessung gestoppt." << std::endl;
            }
            m_context->processFilteredEvent(rawEvent);
            break;

        // --- NEU: REAKTION AUF BANDENDE ---
        case logik::Event::END_BLOCKED:
            m_isEndBlocked = true;
            std::cout << "[EventManager] !!! WICHTIG: Bandende erreicht! Motor-Stopp angefordert." << std::endl;

            // 1. Motor sofort via Dispatcher stoppen, um Herunterfallen zu verhindern!
            m_context->getDispatcher()->driveStop();

            // 2. Event an die FSM leiten
            m_context->processFilteredEvent(rawEvent);
            break;

        case logik::Event::END_CLEARED:
            m_isEndBlocked = false;
            std::cout << "[EventManager] Bandende wieder frei. Motor-Freigabe erteilt." << std::endl;
            m_context->processFilteredEvent(rawEvent);
            break;

        // --- RUTSCHEN-ÜBERWACHUNG ---
        case logik::Event::CHUTE_BLOCKED:
            if (!m_isChuteTimerRunning) {
                m_chuteBlockedStartTime = std::chrono::steady_clock::now();
                m_isChuteTimerRunning = true;
                std::cout << "[EventManager] Rutschen-Lichtschranke belegt. Starte 2-Sekunden-Filter..." << std::endl;
            }
            break;

        case logik::Event::CHUTE_CLEARED:
            if (m_isChuteTimerRunning) {
                m_isChuteTimerRunning = false;
                std::cout << "[EventManager] Rutsche wieder frei. Filter abgebrochen." << std::endl;
                m_context->getErrorHandler()->setLocalChuteStatus(false);
            }
            break;

        // --- START-TASTER-ÜBERWACHUNG (KURZ VS. LANG) ---
        case logik::Event::START_PRESSED:
            if (!m_isStartTimerRunning) {
                m_startPressedStartTime = std::chrono::steady_clock::now();
                m_isStartTimerRunning = true;
                std::cout << "[EventManager] START-Taste gedrueckt. Messung läuft..." << std::endl;
            }
            break;

        case logik::Event::START_RELEASED:
            if (m_isStartTimerRunning) {
                m_isStartTimerRunning = false;

                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - m_startPressedStartTime
                );

                if (duration >= START_LONG_THRESHOLD) {
                    std::cout << "[EventManager] START-Taste LANG gedrueckt (" << duration.count() << "ms)." << std::endl;
                    m_context->processFilteredEvent(logik::Event::START_LONG_PRESSED);
                } else {
                    std::cout << "[EventManager] START-Taste KURZ gedrueckt (" << duration.count() << "ms)." << std::endl;
                    m_context->processFilteredEvent(logik::Event::START_SHORT_PRESSED);
                }
            }
            break;

        // --- DIRECT-EVENTS ---
        default:
            m_context->processFilteredEvent(rawEvent);
            break;
    }
}

// ============================================================================
// ZYKLISCHES UPDATE (WIRD IM TIMER-THREAD ALLE 10MS AUFGERUFEN)
// ============================================================================
void EventManager::update() {
    if (!m_context) return;

    auto now = std::chrono::steady_clock::now();

    // 1. Prüfe Rutschen-Timeout (Rutsche voll)
    if (m_isChuteTimerRunning) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_chuteBlockedStartTime);
        if (duration >= CHUTE_FILTER_TIME) {
            std::cout << "[EventManager] !!! WARNUNG: Rutsche blockiert! Melde RUTSCHE VOLL." << std::endl;
            m_isChuteTimerRunning = false;
            m_context->getErrorHandler()->setLocalChuteStatus(true);
            m_context->processFilteredEvent(logik::Event::LOCAL_CHUTE_FULL);
        }
    }

    // 2. NEU: Prüfe Einlauf-Timeout (Werkstück blockiert am Anfang)
    if (m_isEntryTimerRunning) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_entryBlockedStartTime);
        if (duration >= ENTRY_FILTER_TIME) {
            std::cerr << "[EventManager] !!! FEHLER: Einlauf seit " << duration.count() << "ms blockiert! Loese ERROR aus." << std::endl;
            m_isEntryTimerRunning = false; // Timer stoppen

            // Stoppe sofort das Band, da hier etwas permanent blockiert!
            m_context->getDispatcher()->driveStop();

            // Sende das ERROR Event an die FSM (Ebene 1 springt in den Error-State)
            m_context->processFilteredEvent(logik::Event::ERROR);
        }
    }
}

// ============================================================================
// FILTER-RESET
// ============================================================================
void EventManager::resetFilters() {
    m_isChuteTimerRunning = false;
    m_isStartTimerRunning = false;
    m_isEntryTimerRunning = false;
    m_isEndBlocked = false; // Zurücksetzen bei Anlagen-Reset
    std::cout << "[EventManager] Alle zeitlichen Filter und Sprintsperren zurueckgesetzt." << std::endl;
}

} // namespace logik