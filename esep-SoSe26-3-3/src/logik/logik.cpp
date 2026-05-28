/**
 * @file Logik.cpp
 * @brief Vollständige Implementierung der Test-Logik mit Kalibrierung und Erkennungsmodus.
 */

#include "Logik.hpp"
#include "dispatcher/I_Dispatcher.hpp" // Header für Dispatcher-Aufrufe binden
#include <iostream>

namespace logik {

// ============================================================================
// KONSTRUKTOR & INITIALISIERUNG
// ============================================================================

Logik::Logik()
    : m_position(AnlagenPosition::VORN_ANLAGE_1),
      m_countTicks(false),
      m_tickCounter(0),
      m_dispatcher(nullptr),
      m_isCalibrated(false) { // Am Start ist die Anlage noch unkalibriert
}

Logik::~Logik() {
    // Sicherer, leerer virtueller Destruktor zur Behebung des vtable-Fehlers
}

void Logik::init(AnlagenPosition pos) {
    m_position = pos;
    std::string posName = (pos == AnlagenPosition::VORN_ANLAGE_1) ? "Anlage 1 (Vorn)" : "Anlage 2 (Hinten)";
    std::cout << "[Logik] Initialisiert fuer Position: " << posName << std::endl;
}

// ============================================================================
// CENTRAL EVENT PROCESSING (Zentrale Ereignis-Verarbeitung)
// ============================================================================

void Logik::processEvent(logik::Event e) {
    if (!m_dispatcher) {
        std::cerr << "[Logik] CRITICAL: Kein Dispatcher-Zeiger gesetzt!" << std::endl;
        return;
    }

    switch (e) {

        // 1. START-TASTER: Unterscheidet zwischen Erst-Kalibrierung und Normalbetrieb
        case logik::Event::START_PRESSED:

                std::cout << "[Logik] START -> Modus: KALIBRIERUNG." << std::endl;
				m_isCalibrated = false;
                m_dispatcher->setAdcCalibrationsState();
            break;

        // 2. EINLAUF-LICHTSCHRANKE: Werkstück wird aufgelegt
        case logik::Event::ENTRY_BLOCKED:

                m_dispatcher->driveRight();
                m_tickCounter = 0;
                m_countTicks = true; // Aktiviert die 200-Takt-Bremsrampe in tackt()
            break;

        case logik::Event::ENTRY_CLEARED:
            std::cout << "[Logik] Werkstueck hat den Einlauf verlassen." << std::endl;
            break;

        case logik::Event::HEIGHT_BLOCKED:
            std::cout << "[Logik] Werkstueck hat den Hoehensensor erreicht." << std::endl;
            break;

        case logik::Event::HEIGHT_CLEARED:
            std::cout << "[Logik] Werkstueck hat den Hoehensensor verlassen." << std::endl;
            break;

        // 3. ZIEL-LICHTSCHRANKEN: Werkstück hat das Bandende erreicht
        case logik::Event::END_BLOCKED:
        case logik::Event::CHUTE_BLOCKED:
            std::cout << "[Logik] Ziel erreicht -> Motor STOPP." << std::endl;
            m_dispatcher->driveStop();

            // Wenn wir aus der Erst-Kalibrierung kommen, schalten wir die FSM um!
            if (!m_isCalibrated) {
                m_isCalibrated = true;
                std::cout << "\n==================================================" << std::endl;
                std::cout << " [Logik] >>> KALIBRIERUNG ERFOLGREICH BEENDET! <<<" << std::endl;
                std::cout << "==================================================\n" << std::endl;
            }
            break;

        // Sicherheits- und Notabschaltungen
        case logik::Event::STOP_PRESSED:
            std::cout << "[Logik] setAdcIdelState" << std::endl;
            m_dispatcher->setAdcWaitigForWpState();
            break;

        case logik::Event::ESTOP_PRESSED:
            std::cerr << "[Logik] !!! NOT-HALT AUSGELÖST -> SOFORTIGER HARDWARE-STOPP !!!" << std::endl;
            m_dispatcher->driveStop();
            m_countTicks = false;
            break;

        default:
            break;
    }
}

// ============================================================================
// BAND-TAKTE / TIMING-MANAGEMENT
// ============================================================================

void Logik::tackt() {
    // Wird vom Belt-Timer kontinuierlich (z.B. alle 10ms) aufgerufen

    if (m_countTicks && m_dispatcher) {
        m_tickCounter++;

        // Nur im Kalibriermodus: Nach 200 Takten bremsen, bevor das Teil unter den Sensor rutscht
        if (m_tickCounter >= 200) {

			    if (m_isCalibrated) {
 					std::cout << "[Logik-Kalib] set Adc Waitig For WpState." << std::endl;
					m_dispatcher->setAdcWaitigForWpState();
				}
            m_dispatcher->driveSlowRight();

            m_countTicks = false;
            m_tickCounter = 0;
        }
    }
}

// ============================================================================
// RÜCKMELDUNG VOM PROFILEANALYZER (Ergebnis des 12-Segment-Scans)
// ============================================================================

void Logik::workpieceTypeFromHS(WpType type) {
    // Hier liest die Logik das Ergebnis deiner 'evaluationFinished'-Methode aus!
    std::string name = "UNKNOWN";
    switch (type) {
        case WpType::FLAT:         name = "FLAT (Flach)"; break;
        case WpType::TALL:         name = "TALL (Hoch)"; break;
        case WpType::TALL_DRILLED: name = "TALL_DRILLED (Gebohrt)"; break;
        default:                   name = "UNBEKANNT / FEHLER"; break;
    }

    std::cout << "\n==================================================" << std::endl;
    std::cout << " [LOGIK-AUSWERTUNG] Werkstueck-Typ ist: " << name << std::endl;
    std::cout << "==================================================\n" << std::endl;
}

// ============================================================================
// UNGENUTZTE INTERFACE-METHODEN (Als Stubs ausformuliert)
// ============================================================================

void Logik::workpieceInfo(int id, WpType type) {}
void Logik::updateWpListe(WpType type) {}
void Logik::setTrackedSegmentTicks(uint16_t calibMode, uint16_t tickValue) {}
void Logik::receiveRawCalibrationData(uint16_t calibMode, uint16_t adcValue) {}

} // namespace logik