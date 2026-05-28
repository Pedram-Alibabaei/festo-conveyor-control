/**
 * @file ProfileAnalyzer.cpp
 * @brief Logische Zustandsschaltung und Auswertung der ADC-Höhenprofile.
 */

#include "ProfileAnalyzer.hpp"
#include "shared/ipc_definitions.hpp"
#include <iostream>
#include <shared/types.hpp>

extern "C" {
#include <sys/neutrino.h>
}

// ============================================================================
// KONSTRUKTOR & KONFIGURATION
// ============================================================================

ProfileAnalyzer::ProfileAnalyzer()
    : m_coid(-1),
      m_currentState(AnalyzerState::IDLE),
      m_currentCalibStep(CalibStep::MEASURE_BELT),
      m_maxHeightInCurrentProfile(0),
      m_sampleCounter(0),
      m_tempBeltValue(3500),
      m_fillCount(0),
      m_currentSegmentIndex(0) {

    for (size_t i = 0; i < SMOOTHING_SAMPLE_COUNT; ++i) {
        m_rawBuffer[i] = 0;
    }
    // 12 Segmente mit UNKNOWN initialisieren
    for (size_t i = 0; i < NUM_PROFILE_CLASSES; ++i) {
        m_segmentTypes[i] = static_cast<uint16_t>(WpType::UNKNOWN);
    }
}

void ProfileAnalyzer::setConnectionId(int coid) {
    m_coid = coid;
}

// ============================================================================
// LOGISCHE ZUSTANDSTEUERUNG (Aufgerufen durch HalSensors)
// ============================================================================

void ProfileAnalyzer::setAdcIdelState() {
    m_currentState = AnalyzerState::IDLE;
}

void ProfileAnalyzer::setAdcWaitigForWpState() {
    m_currentState = AnalyzerState::WAITING_FOR_WP;
}

void ProfileAnalyzer::setAdcCalibrationsState() {
    m_currentState = AnalyzerState::CALIBRATION;
    m_currentCalibStep = CalibStep::MEASURE_BELT; // Kette immer von vorne starten!
    m_maxHeightInCurrentProfile = 0;
    m_sampleCounter = 0;
}

// ============================================================================
// HILFSMETHODE: MITTELWERT BERECHNEN
// ============================================================================
uint32_t ProfileAnalyzer::calculateBlockAverage() {
    uint64_t sum = 0;
    for (size_t i = 0; i < SMOOTHING_SAMPLE_COUNT; ++i) {
        sum += m_rawBuffer[i];
    }
    return static_cast<uint32_t>(sum / SMOOTHING_SAMPLE_COUNT);
}

// ============================================================================
// VERARBEITUNG DER SAMPLER (Aus der Interrupt Service Routine)
// ============================================================================

void ProfileAnalyzer::processSample(uint32_t adcValue) {
    if (m_coid == -1) return;

    // ========================================================================
    // 1. BLOCK-GLÄTTUNG (Sammle erst 10 Rohwerte im festen Array)
    // ========================================================================
    m_rawBuffer[m_fillCount] = adcValue;
    m_fillCount++;

    if (m_fillCount < SMOOTHING_SAMPLE_COUNT) {
        return;
    }

    uint32_t smoothedValue = calculateBlockAverage();
    m_fillCount = 0;

    uint16_t currentRaw = static_cast<uint16_t>(smoothedValue & 0xFFF);

    // ========================================================================
    // 2. DIE ZUSTANDSMASCHINE
    // ========================================================================
    switch (m_currentState) {

        case AnalyzerState::IDLE:
            break;

        case AnalyzerState::WAITING_FOR_WP:
            {
                uint16_t emptyBeltThreshold = m_thresholds.count(static_cast<uint16_t>(ipc::CalibMode::EMPTY_BELT)) ?
                                              m_thresholds[static_cast<uint16_t>(ipc::CalibMode::EMPTY_BELT)] : 3500;

                if (currentRaw < (emptyBeltThreshold - 100)) {
                    m_currentState = AnalyzerState::MEASURING;
                    m_maxHeightInCurrentProfile = currentRaw;
                    m_sampleCounter = 1;
                    m_currentSegmentIndex = 0;
                    m_currentSegmentBuffer.clear();

                    for (size_t i = 0; i < NUM_PROFILE_CLASSES; ++i) {
                        m_segmentTypes[i] = static_cast<uint16_t>(WpType::UNKNOWN);
                    }
                }
            }
            break;

        case AnalyzerState::MEASURING:
            {
                // ------------------------------------------------------------
                // CRITICAL CHECK: MAX_HEIGHT (Höhenfehler-Überwachung)
                // ------------------------------------------------------------
                uint16_t highErrorLimit = m_thresholds[static_cast<uint16_t>(ipc::CalibMode::MAX_HEIGHT)];

                if (currentRaw < highErrorLimit) {
                    std::cerr << "[Analyzer] CRITICAL: Werkstueck zu hoch! ADC: " << currentRaw << std::endl;

                    uint32_t payload = ipc::encodeHalPayload(ipc::SensorID::HEIGHT, ipc::State::HIGH_ERROR);
                    MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_INPUT, payload);

                    m_currentState = AnalyzerState::IDLE;
                    break;
                }

                // ------------------------------------------------------------
                // NORMALER MESSABLAUF
                // ------------------------------------------------------------
                uint16_t emptyBeltThreshold = m_thresholds[static_cast<uint16_t>(ipc::CalibMode::EMPTY_BELT)];

                if (currentRaw < (emptyBeltThreshold - 100)) {
                    m_sampleCounter++;

                    if (currentRaw < m_maxHeightInCurrentProfile) {
                        m_maxHeightInCurrentProfile = currentRaw;
                    }

                    uint16_t minSamples = m_thresholds[static_cast<uint16_t>(ipc::CalibMode::MIN_SAMPLES)];
                    size_t samplesPerSegment = (minSamples > 0) ? (minSamples / NUM_PROFILE_CLASSES) : 10;

                    m_currentSegmentBuffer.push_back(currentRaw);

                    if (m_currentSegmentBuffer.size() >= samplesPerSegment && m_currentSegmentIndex < NUM_PROFILE_CLASSES) {
                        m_segmentTypes[m_currentSegmentIndex] = determineSegmentType(m_currentSegmentBuffer);
                        m_currentSegmentIndex++;
                        m_currentSegmentBuffer.clear();
                    }
                }
                else {
                    // ------------------------------------------------------------
                    // WERKSTÜCK HAT SENSOR VERLASSEN
                    // ------------------------------------------------------------
                    if (!m_currentSegmentBuffer.empty() && m_currentSegmentIndex < NUM_PROFILE_CLASSES) {
                        m_segmentTypes[m_currentSegmentIndex] = determineSegmentType(m_currentSegmentBuffer);
                    }

                    uint16_t minSamples = m_thresholds[static_cast<uint16_t>(ipc::CalibMode::MIN_SAMPLES)];
                    if (m_sampleCounter >= (minSamples * 0.7)) {
                        uint16_t finalWpType = determineFinalWorkpieceType();
                        evaluationFinished(finalWpType);
                    } else {
                        std::cout << "[Analyzer] Messung verworfen (zu kurz oder Rauschen)." << std::endl;
                    }

                    m_currentState = AnalyzerState::IDLE;
                }
            }
            break;

        case AnalyzerState::CALIBRATION:
            {
                using CM = ipc::CalibMode;

                switch (m_currentCalibStep) {

                    // --------------------------------------------------------
                    // STUFE 1: LEERES BAND EINMESSEN (50 Samples)
                    // --------------------------------------------------------
                    case CalibStep::MEASURE_BELT:
                        m_sampleCounter++;
                        if (m_sampleCounter == 1) {
                            m_maxHeightInCurrentProfile = currentRaw;
                        } else {
                            m_maxHeightInCurrentProfile = (m_maxHeightInCurrentProfile + currentRaw) / 2;
                        }

                        if (m_sampleCounter >= 50) {
                            m_tempBeltValue = m_maxHeightInCurrentProfile;

                            uint32_t payload = ipc::encodeCalibPayload(m_tempBeltValue, static_cast<uint16_t>(CM::EMPTY_BELT));
                            MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_ADC_CALIBRATION, payload);

                            std::cout << "[Calib] Stufe 1 fertig: EMPTY_BELT = " << m_tempBeltValue << ". Warte auf WP..." << std::endl;

                            m_currentCalibStep = CalibStep::WAIT_FOR_WP;
                            m_maxHeightInCurrentProfile = 4095;
                            m_sampleCounter = 0;
                        }
                        break;

                    // --------------------------------------------------------
                    // STUFE 2: WARTEN AUF FLACHES WERKSTÜCK & PEAK ERFASSEN
                    // --------------------------------------------------------
                    case CalibStep::WAIT_FOR_WP:
                        if (currentRaw < (m_tempBeltValue - 100)) {
                            m_sampleCounter++;

                            if (currentRaw < m_maxHeightInCurrentProfile) {
                                m_maxHeightInCurrentProfile = currentRaw;
                            }

                            if (m_sampleCounter == 15) {
                                uint32_t payload = ipc::encodeCalibPayload(m_maxHeightInCurrentProfile, static_cast<uint16_t>(CM::FLAT_WP));
                                MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_ADC_CALIBRATION, payload);

                                std::cout << "[Calib] Stufe 2 fertig: FLAT_WP = " << m_maxHeightInCurrentProfile << ". Erfasse Gesamtlänge..." << std::endl;

                                m_currentCalibStep = CalibStep::WAIT_FOR_LEAVE;
                            }
                        }
                        break;

                    // --------------------------------------------------------
                    // STUFE 3: WEITERZÄHLEN, BIS WERKSTÜCK DAS BAND VERLÄSST
                    // --------------------------------------------------------
                    case CalibStep::WAIT_FOR_LEAVE:
                        m_sampleCounter++;

                        if (currentRaw >= (m_tempBeltValue - 80)) {
                            uint16_t totalSamples = static_cast<uint16_t>(m_sampleCounter);

                            uint32_t payload = ipc::encodeCalibPayload(totalSamples, static_cast<uint16_t>(CM::MIN_SAMPLES));
                            MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_ADC_CALIBRATION, payload);

                            std::cout << "[Calib] Stufe 3 fertig: MIN_SAMPLES = " << totalSamples << ". Kalibrierung beendet!" << std::endl;

                            m_currentState = AnalyzerState::IDLE;
                        }
                        break;
                }
            }
            break;
    }
}

// ============================================================================
// HILFSMETHODEN
// ============================================================================

void ProfileAnalyzer::evaluationFinished(uint16_t finalTypeOrValue) {
    uint32_t payload = ipc::encodeHalPayload(ipc::SensorID::HEIGHT, finalTypeOrValue);
    MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_INPUT, payload);
}

void ProfileAnalyzer::updateCalibrationThresholds(uint16_t calib_mode, uint16_t calib_value) {
    m_thresholds[calib_mode] = calib_value;

    std::cout << "[Analyzer] Threshold aktualisiert: Mode=" << calib_mode
              << " Value=" << calib_value << std::endl;

    if (calib_mode == static_cast<uint16_t>(ipc::CalibMode::FLAT_WP)) {
        recalculateTypeThresholds(calib_value);
    }
}

void ProfileAnalyzer::recalculateTypeThresholds(uint16_t flatCenter) {
    using CM = ipc::CalibMode;

    m_thresholds[static_cast<uint16_t>(CM::FLAT_MIN)]    = flatCenter - 50;
    m_thresholds[static_cast<uint16_t>(CM::FLAT_MAX)]    = flatCenter + 50;

    m_thresholds[static_cast<uint16_t>(CM::TALL_MIN)]    = flatCenter - 350;
    m_thresholds[static_cast<uint16_t>(CM::TALL_MAX)]    = flatCenter - 200;

    m_thresholds[static_cast<uint16_t>(CM::DRILLED_MIN)] = flatCenter - 850;
    m_thresholds[static_cast<uint16_t>(CM::DRILLED_MAX)] = flatCenter - 650;

    m_thresholds[static_cast<uint16_t>(CM::MAX_HEIGHT)]  = flatCenter - 300;

    std::cout << "[Analyzer] Schwellenwerte via Enum Class berechnet." << std::endl;
}

uint16_t ProfileAnalyzer::determineSegmentType(const std::vector<uint16_t>& segmentSamples) {
    if (segmentSamples.empty()) return static_cast<uint16_t>(WpType::UNKNOWN);

    using CM = ipc::CalibMode;

    uint16_t segmentPeak = 4095;
    for (uint16_t val : segmentSamples) {
        if (val < segmentPeak) {
            segmentPeak = val;
        }
    }

    uint16_t flatMin    = m_thresholds[static_cast<uint16_t>(CM::FLAT_MIN)];
    uint16_t flatMax    = m_thresholds[static_cast<uint16_t>(CM::FLAT_MAX)];
    uint16_t tallMin    = m_thresholds[static_cast<uint16_t>(CM::TALL_MIN)];
    uint16_t tallMax    = m_thresholds[static_cast<uint16_t>(CM::TALL_MAX)];
    uint16_t drilledMin = m_thresholds[static_cast<uint16_t>(CM::DRILLED_MIN)];
    uint16_t drilledMax = m_thresholds[static_cast<uint16_t>(CM::DRILLED_MAX)];

    if (segmentPeak >= drilledMin && segmentPeak <= drilledMax) {
        return static_cast<uint16_t>(WpType::TALL_DRILLED);
    }
    if (segmentPeak >= tallMin && segmentPeak <= tallMax) {
        return static_cast<uint16_t>(WpType::TALL);
    }
    if (segmentPeak >= flatMin && segmentPeak <= flatMax) {
        return static_cast<uint16_t>(WpType::FLAT);
    }

    return static_cast<uint16_t>(WpType::UNKNOWN);
}

uint16_t ProfileAnalyzer::determineFinalWorkpieceType() {
    using WT = WpType;

    bool allTall = true;
    bool allFlat = true;

    for (size_t i = 0; i < NUM_PROFILE_CLASSES; ++i) {
        if (m_segmentTypes[i] != static_cast<uint16_t>(WT::TALL)) {
            allTall = false;
        }
        if (m_segmentTypes[i] != static_cast<uint16_t>(WT::FLAT)) {
            allFlat = false;
        }
    }

    if (allTall) {
        std::cout << "[Analyzer] Profil-Muster passt exakt auf: TALL" << std::endl;
        return static_cast<uint16_t>(WT::TALL);
    }

    if (allFlat) {
        std::cout << "[Analyzer] Profil-Muster passt exakt auf: FLAT" << std::endl;
        return static_cast<uint16_t>(WT::FLAT);
    }

    bool outerIsTall = (m_segmentTypes[0] == static_cast<uint16_t>(WT::TALL)) &&
                       (m_segmentTypes[1] == static_cast<uint16_t>(WT::TALL)) &&
                       (m_segmentTypes[9] == static_cast<uint16_t>(WT::TALL)) &&
                       (m_segmentTypes[10] == static_cast<uint16_t>(WT::TALL));

    bool centerIsDrilled = (m_segmentTypes[5] == static_cast<uint16_t>(WT::TALL_DRILLED)) &&
                           (m_segmentTypes[6] == static_cast<uint16_t>(WT::TALL_DRILLED));

    if (outerIsTall && centerIsDrilled) {
        std::cout << "[Analyzer] Profil-Muster passt exakt auf: TALL_DRILLED" << std::endl;
        return static_cast<uint16_t>(WT::TALL_DRILLED);
    }

    std::cout << "[Analyzer] Warnung: Unbekanntes Profil-Muster!" << std::endl;
    return static_cast<uint16_t>(WT::UNKNOWN);
}
