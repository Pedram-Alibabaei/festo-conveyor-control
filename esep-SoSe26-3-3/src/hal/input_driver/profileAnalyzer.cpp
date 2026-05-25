/**
 * @file ProfileAnalyzer.cpp
 * @brief Logische Zustandsschaltung und Auswertung der ADC-Höhenprofile.
 */

#include "ProfileAnalyzer.hpp"
#include "shared/ipc_definitions.hpp"
#include <iostream>

extern "C" {
#include <sys/neutrino.h>
}

// ============================================================================
// KONSTRUKTOR & KONFIGURATION
// ============================================================================

ProfileAnalyzer::ProfileAnalyzer()
    : m_coid(-1),
      m_currentState(AnalyzerState::IDLE),
      m_sampleCounter(0) {
}

void ProfileAnalyzer::setConnectionId(int coid) {
    m_coid = coid;
}

// ============================================================================
// LOGISCHE ZUSTANDSTEUERUNG (Aufgerufen durch HalSensors)
// ============================================================================

void ProfileAnalyzer::setAdcIdelState() {
    m_currentState = AnalyzerState::IDLE;
    m_sampleCounter = 0;
}

void ProfileAnalyzer::setAdcWaitigForWpState() {
    m_currentState = AnalyzerState::WAITING_FOR_WP;
    m_sampleCounter = 0;
}

void ProfileAnalyzer::setAdcCalibrationsState() {
    m_currentState = AnalyzerState::CALIBRATION;
    m_sampleCounter = 0;
}

void ProfileAnalyzer::updateCalibrationThresholds(uint16_t calib_mode, uint16_t calib_value) {
    m_thresholds[calib_mode] = calib_value;
}

// ============================================================================
// VERARBEITUNG DER SAMPLER (Aus der Interrupt Service Routine)
// ============================================================================

void ProfileAnalyzer::processSample(uint32_t adcValue) {
    // Falls die COID noch nicht gesetzt ist, ignorieren wir die Samples sicherheitshalber
    if (m_coid == -1) return;

    uint16_t currentRaw = static_cast<uint16_t>(adcValue & 0xFFF); // 12-Bit ADC Auflösung

    switch (m_currentState) {

        case AnalyzerState::IDLE:
            // Im Idle-Zustand wird die Hardware zwar getriggert, wir werten aber nichts aus
            break;

        case AnalyzerState::WAITING_FOR_WP:
            // Hole den gelernten Schwellenwert für das leere Band (Default: z.B. 3500)
            // Wenn der Wert sinkt, befindet sich ein Werkstück unter dem Sensor
            {
                uint16_t emptyBeltThreshold = m_thresholds.count(ipc::CalibMode::EMPTY_BELT) ?
                                              m_thresholds[ipc::CalibMode::EMPTY_BELT] : 3500;

                // Werkstück bricht die Lichtschranke / senkt den ADC-Wert unter das Toleranzband
                if (currentRaw < (emptyBeltThreshold - 100)) {
                    m_currentState = AnalyzerState::MEASURING;
                    m_maxHeightInCurrentProfile = currentRaw; // Ersten Messwert sichern
                    m_sampleCounter = 1;
                }
            }
            break;

        case AnalyzerState::MEASURING:
            {
                uint16_t emptyBeltThreshold = m_thresholds.count(ipc::CalibMode::EMPTY_BELT) ?
                                              m_thresholds[ipc::CalibMode::EMPTY_BELT] : 3500;

                // Solange das Werkstück da ist, tracken wir das Profil (z.B. den Peak/kleinsten ADC-Wert)
                if (currentRaw < (emptyBeltThreshold - 100)) {
                    m_sampleCounter++;
                    // Da der Sensor invertiert arbeitet (Höher = kleinerer ADC-Wert),
                    // suchen wir hier für das Maximum das mathematische Minimum des Rohwerts.
                    if (currentRaw < m_maxHeightInCurrentProfile) {
                        m_maxHeightInCurrentProfile = currentRaw;
                    }
                }
                else {
                    // Werkstück hat den ADC-Sensor verlassen -> Auswertung abschließen!
                    // Hier würde jetzt die Typermittlung anhand der Schwellenwerte stattfinden (z.B. WP_FLAT)
                    uint16_t detectedType = 1; // Dummy-Typ für die Logik (Normaler HAL_INPUT Puls-Payload)

                    evaluationFinished(detectedType);
                    m_currentState = AnalyzerState::IDLE; // Zurück in den Idle, bis Logik wieder freigibt
                }
            }
            break;

        case AnalyzerState::CALIBRATION:
            // LIVE-KALIBRIERUNG (Wenn keine Datei existierte)
            // Die Logik steuert, welches Objekt gerade eingelernt wird. Der Analyzer sammelt die Rohwerte
            // und schiebt sie per HAL_ADC_CALIBRATION unverarbeitet hoch zur Auswertung in die Logik-FSM.
            {
                m_sampleCounter++;
                if (currentRaw < m_maxHeightInCurrentProfile || m_maxHeightInCurrentProfile == 0) {
                    m_maxHeightInCurrentProfile = currentRaw;
                }

                // Ein kurzes Sample-Fenster reicht zum Einlernen (z.B. 50 Samples stabil)
                if (m_sampleCounter >= 50) {
                    // Wir schicken den ermittelten Extremwert im passenden Live-Modus hoch.
                    // Welcher Modus das ist (EMPTY_BELT oder FLAT_WP), weiß die Logik-FSM anhand ihres Zustands.
                    uint32_t payload = ipc::encodeCalibPayload(m_maxHeightInCurrentProfile, ipc::CalibMode::EMPTY_BELT);

                    MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_ADC_CALIBRATION, payload);

                    // Um ein Überfluten zu verhindern, gehen wir nach dem Absenden in den Idle
                    m_currentState = AnalyzerState::IDLE;
                }
            }
            break;
    }
}

// ============================================================================
// HILFSMETHODEN
// ============================================================================

void ProfileAnalyzer::evaluationFinished(uint16_t finalTypeOrValue) {
    // Ein normales Werkstück wurde im laufenden Betrieb vermessen
    // Packen: [ 16 Bit: Zustand/Typ ] [ 16 Bit: SensorID ]
    uint32_t payload = ipc::encodeHalPayload(ipc::SensorID::HEIGHT, finalTypeOrValue);

    // Normaler HAL_INPUT Puls an den Dispatcher abfeuern
    MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_INPUT, payload);
}