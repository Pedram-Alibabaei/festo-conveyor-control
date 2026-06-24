/**
 * @file ProfileAnalyzer.cpp
 * @brief Logische Zustandsschaltung und Auswertung der ADC-Hoehenprofile.
 */

#include "ProfileAnalyzer.hpp"
#include "shared/ipc_definitions.hpp"
#include "shared/types.hpp"

#include <algorithm>
#include <iostream>

extern "C" {
#include <sys/neutrino.h>
}

namespace
{
constexpr uint16_t ADC_MAX_VALUE = 4095;

constexpr uint16_t TRIGGER_OFFSET = 200;
constexpr uint16_t RELEASE_OFFSET = 100;

constexpr size_t BELT_REFERENCE_SAMPLE_COUNT = 100;
constexpr size_t FLAT_REFERENCE_SAMPLE_COUNT = 100;
constexpr size_t CALIBRATION_TRIGGER_CONFIRM_COUNT = 20;
constexpr size_t CALIBRATION_RELEASE_CONFIRM_COUNT = 20;
constexpr uint16_t DEFAULT_MIN_SAMPLES = 120;
constexpr size_t MIN_MEASUREMENT_PERCENT = 80;
constexpr size_t MAIN_CLASS_PERCENT = 50;
constexpr size_t DRILLED_CLASS_PERCENT = 10;
constexpr uint32_t UNKNOWN_DOMINANCE_PERCENT = 35;

constexpr uint16_t FLAT_TOLERANCE = 50;
constexpr uint16_t TALL_CENTER_OFFSET = 275;
constexpr uint16_t TALL_TOLERANCE = 75;
constexpr uint16_t DRILLED_CENTER_OFFSET = 750;
constexpr uint16_t DRILLED_TOLERANCE = 100;
constexpr uint16_t MAX_HEIGHT_MARGIN = 150;

uint16_t safeSubtract(uint16_t value, uint16_t offset)
{
    return value > offset ? static_cast<uint16_t>(value - offset) : 0;
}

uint16_t safeAdd(uint16_t value, uint16_t offset)
{
    const uint32_t result = static_cast<uint32_t>(value) + offset;
    return result > ADC_MAX_VALUE ? ADC_MAX_VALUE : static_cast<uint16_t>(result);
}

uint16_t averageAsUInt16(uint32_t sum, size_t count)
{
    return count == 0 ? 0 : static_cast<uint16_t>((sum + (count / 2)) / count);
}

size_t percentCeil(size_t value, size_t percent)
{
    return (value * percent + 99) / 100;
}
} // namespace

// KONSTRUKTOR & INITIALISIERUNG

ProfileAnalyzer::ProfileAnalyzer()
    : m_threshMinSamples(DEFAULT_MIN_SAMPLES)
{
    resetSmoothingBuffer();
    resetMeasurementData();
}

void ProfileAnalyzer::setConnectionId(int coid)
{
    m_coid = coid;
}

void ProfileAnalyzer::setAdcIdelState()
{
    std::cout << "[Analyzer] State -> IDLE" << std::endl;
    m_currentState = AnalyzerState::IDLE;
    m_serviceTimingOnly = false;
    m_waitForTunnelClear = false;
    m_nextWaitingForWpIsServiceTiming = false;
    m_transportPaused = false;
    m_heightErrorActive = false;
    resetMeasurementData();
    resetSmoothingBuffer();
}

void ProfileAnalyzer::setAdcWaitigForWpState()
{
    if (m_currentState == AnalyzerState::MEASURING_PAUSED ||
        (m_currentState == AnalyzerState::MEASURING && m_transportPaused)) {
        std::cout << "[Analyzer] WAITING_FOR_WP ignoriert: pausierte Messung bleibt erhalten." << std::endl;
        return;
    }

    resetMeasurementData();
    resetSmoothingBuffer();
    m_serviceTimingOnly = m_nextWaitingForWpIsServiceTiming;
    m_waitForTunnelClear = false;
    m_nextWaitingForWpIsServiceTiming = false;
    m_transportPaused = false;
    m_heightErrorActive = false;
    m_currentState = AnalyzerState::WAITING_FOR_WP;
    std::cout << "[Analyzer] State -> "
              << (m_serviceTimingOnly ? "SERVICE_TIMING" : "WAITING_FOR_WP")
              << std::endl;

    if (!isCalibrated()) {
        std::cerr << "[Analyzer] Warnung: WAITING_FOR_WP ohne vollstaendige ADC-Kalibrierung." << std::endl;
    }
}

void ProfileAnalyzer::setAdcCalibrationsState()
{
    m_currentState = AnalyzerState::CALIBRATION;
    m_currentCalibStep = CalibStep::MEASURE_BELT;
    m_serviceTimingOnly = false;
    m_waitForTunnelClear = false;
    m_nextWaitingForWpIsServiceTiming = false;
    m_transportPaused = false;
    m_heightErrorActive = false;
    resetSmoothingBuffer();
    resetMeasurementData();
    resetCalibrationData();
    std::cout << "[Analyzer] State -> CALIBRATION" << std::endl;
}

void ProfileAnalyzer::setServiceTimingMeasureState()
{
    m_nextWaitingForWpIsServiceTiming = true;
    setAdcWaitigForWpState();
    std::cout << "[Analyzer] Service-Timing-Messung aktiviert (HEIGHT_BLOCKED/CLEARED)." << std::endl;
}

void ProfileAnalyzer::resetSmoothingBuffer()
{
    std::fill(m_rawBuffer, m_rawBuffer + SMOOTHING_SAMPLE_COUNT, 0);
    m_fillCount = 0;
}

void ProfileAnalyzer::resetMeasurementData()
{
    m_transportPaused = false;
    m_sampleCounter = 0;
    m_flatCounter = 0;
    m_tallCounter = 0;
    m_drilledCounter = 0;
    m_unknownCounter = 0;
    m_measurementMinAdc = ADC_MAX_VALUE;
    m_measurementMaxAdc = 0;
    m_triggerConfirmCounter = 0;
    m_releaseConfirmCounter = 0;
    m_triggerCandidateBuffer.clear();
    resetTallDrilledPattern();
}

void ProfileAnalyzer::resetCalibrationData()
{
    m_beltSampleSum = 0;
    m_flatSampleSum = 0;
    m_flatSampleCount = 0;
    m_flatReferenceSent = false;
}

void ProfileAnalyzer::resetTallDrilledPattern()
{
    m_tallDrilledPatternState = TallDrilledPatternState::SEARCH_TALL_BEFORE;
    m_isTallDrilled = false;
    m_tallBeforeDrilledCounter = 0;
    m_drilledPatternCounter = 0;
    m_tallAfterDrilledCounter = 0;
}

uint32_t ProfileAnalyzer::calculateBlockAverage()
{
    uint64_t sum = 0;
    for (size_t i = 0; i < SMOOTHING_SAMPLE_COUNT; ++i) {
        sum += m_rawBuffer[i];
    }

    return static_cast<uint32_t>(sum / SMOOTHING_SAMPLE_COUNT);
}

void ProfileAnalyzer::setTransportPaused(bool paused)
{
    if (m_transportPaused == paused) {
        return;
    }

    m_transportPaused = paused;
    m_releaseConfirmCounter = 0;

    if (m_transportPaused && (m_currentState == AnalyzerState::MEASURING)) {
        m_currentState = AnalyzerState::MEASURING_PAUSED;
        std::cout << "[Analyzer] State -> MEASURING_PAUSED" << std::endl;
    } else if (!m_transportPaused &&
               (m_currentState == AnalyzerState::MEASURING_PAUSED)) {
        m_currentState = AnalyzerState::MEASURING;
        std::cout << "[Analyzer] State -> MEASURING" << std::endl;
        resetSmoothingBuffer();
    } else if (!m_transportPaused) {
        resetSmoothingBuffer();
    }

    std::cout << "[Analyzer] Transport "
              << (m_transportPaused ? "pausiert" : "aktiv")
              << " fuer Profilanalyse." << std::endl;
}

// SAMPLER VERARBEITUNG

void ProfileAnalyzer::processSample(uint32_t adcValue)
{
    if (m_coid == -1) {
        return;
    }

    if (m_fillCount >= SMOOTHING_SAMPLE_COUNT) {
        m_fillCount = 0;
    }

    m_rawBuffer[m_fillCount] = adcValue & ADC_MAX_VALUE;
    ++m_fillCount;

    if (m_fillCount < SMOOTHING_SAMPLE_COUNT) {
        return;
    }

    const uint16_t currentRaw = static_cast<uint16_t>(calculateBlockAverage() & ADC_MAX_VALUE);
    m_fillCount = 0;

    switch (m_currentState) {
    case AnalyzerState::IDLE:
        handleHeightError(currentRaw);
        break;

    case AnalyzerState::WAITING_FOR_WP:
        if (handleHeightError(currentRaw)) {
            m_triggerConfirmCounter = 0;
            m_triggerCandidateBuffer.clear();
            break;
        }

    	if (!isCalibrated()) {
            break;
        }

        if (m_waitForTunnelClear) {
            if (currentRaw > m_releaseTriggerValue) {
                ++m_releaseConfirmCounter;
                if (m_releaseConfirmCounter >= runningReleaseConfirmCount()) {
                    m_waitForTunnelClear = false;
                    m_releaseConfirmCounter = 0;
                    m_triggerConfirmCounter = 0;
                    m_triggerCandidateBuffer.clear();
                }
            } else {
                m_releaseConfirmCounter = 0;
            }
            break;
        }

        if (currentRaw <= m_triggerValue) {
            ++m_triggerConfirmCounter;
            m_triggerCandidateBuffer.push_back(currentRaw);

            if (m_triggerConfirmCounter >= runningTriggerConfirmCount()) {
                std::cout << "[Analyzer] Werkstueck an HS bestaetigt -> Sende HEIGHT_BLOCKED" << std::endl;
                beginMeasurement();

                for (uint16_t candidate : m_triggerCandidateBuffer) {
                    addMeasurementSample(candidate);
                }
                m_triggerCandidateBuffer.clear();
            }
        } else {
            m_triggerConfirmCounter = 0;
            m_triggerCandidateBuffer.clear();
        }
        break;

    case AnalyzerState::MEASURING:
        if (handleHeightError(currentRaw)) {
            m_currentState = AnalyzerState::IDLE;
            std::cout << "[Analyzer] State -> IDLE" << std::endl;
            resetMeasurementData();
            break;
        }

        if (m_transportPaused) {
            m_currentState = AnalyzerState::MEASURING_PAUSED;
            std::cout << "[Analyzer] State -> MEASURING_PAUSED" << std::endl;
            break;
        }

        if (currentRaw <= m_releaseTriggerValue) {
            m_releaseConfirmCounter = 0;
            addMeasurementSample(currentRaw);
        } else {
            ++m_releaseConfirmCounter;

            if (m_releaseConfirmCounter >= runningReleaseConfirmCount()) {
                finishMeasurement();
            }
        }
        break;

    case AnalyzerState::MEASURING_PAUSED:
        if (handleHeightError(currentRaw)) {
            m_currentState = AnalyzerState::IDLE;
            std::cout << "[Analyzer] State -> IDLE" << std::endl;
            resetMeasurementData();
            break;
        }

        // Transport steht: Messdaten bleiben erhalten, neue ADC-Werte werden ignoriert.
        break;

    case AnalyzerState::CALIBRATION:
        {
            using CM = ipc::CalibMode;

            switch (m_currentCalibStep) {
            case CalibStep::MEASURE_BELT:
                m_beltSampleSum += currentRaw;
                ++m_sampleCounter;

                if (m_sampleCounter == 1) {
                    std::cout << "[Calib] Leerband-Messung gestartet..." << std::endl;
                } else if (m_sampleCounter % 25 == 0) {
                    std::cout << "[Calib] Leerband-Fortschritt: "
                              << m_sampleCounter << "/"
                              << BELT_REFERENCE_SAMPLE_COUNT << " Samples"
                              << std::endl;
                }

                if (m_sampleCounter >= BELT_REFERENCE_SAMPLE_COUNT) {
                    m_threshEmptyBelt =
                        averageAsUInt16(m_beltSampleSum, BELT_REFERENCE_SAMPLE_COUNT);
                    updateCalibrationThresholds(static_cast<uint16_t>(CM::EMPTY_BELT),
                                                m_threshEmptyBelt);

                    const uint32_t payload =
                        ipc::encodeCalibPayload(m_threshEmptyBelt,
                                                static_cast<uint16_t>(CM::EMPTY_BELT));
                    MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT,
                                 ipc::PulseCode::HAL_ADC_CALIBRATION, payload);
                    std::cout << "[Calib] Stufe 1 fertig: EMPTY_BELT = "
                              << m_threshEmptyBelt
                              << " | Trigger = " << m_triggerValue
                              << " | Release = " << m_releaseTriggerValue
                              << ". Warte auf WP..." << std::endl;

                    m_currentCalibStep = CalibStep::WAIT_FOR_WP;
                    m_sampleCounter = 0;
                    m_triggerConfirmCounter = 0;
                }
                break;

            case CalibStep::WAIT_FOR_WP:
                if (currentRaw <= m_triggerValue) {
                    ++m_triggerConfirmCounter;
                    m_flatSampleSum += currentRaw;
                    ++m_flatSampleCount;

                    if (m_triggerConfirmCounter >= CALIBRATION_TRIGGER_CONFIRM_COUNT) {
                        std::cout << "[Calib] Flaches Referenz-WP erkannt. Erfasse FlatCenter..."
                                  << std::endl;
                        sendHeightState(ipc::State::BLOCKED);
                        m_currentCalibStep = CalibStep::WAIT_FOR_LEAVE;
                        m_sampleCounter = CALIBRATION_TRIGGER_CONFIRM_COUNT;
                        m_releaseConfirmCounter = 0;
                        m_flatReferenceSent = false;
                    }
                } else {
                    m_triggerConfirmCounter = 0;
                    m_flatSampleSum = 0;
                    m_flatSampleCount = 0;
                }
                break;

            case CalibStep::WAIT_FOR_LEAVE:
                if (currentRaw <= m_releaseTriggerValue) {
                    m_releaseConfirmCounter = 0;
                    ++m_sampleCounter;

                    if (m_flatSampleCount < FLAT_REFERENCE_SAMPLE_COUNT) {
                        m_flatSampleSum += currentRaw;
                        ++m_flatSampleCount;

                        if (m_flatSampleCount >= FLAT_REFERENCE_SAMPLE_COUNT) {
                            m_threshFlatWp =
                                averageAsUInt16(m_flatSampleSum, m_flatSampleCount);
                            updateCalibrationThresholds(static_cast<uint16_t>(CM::FLAT_WP),
                                                        m_threshFlatWp);

                            const uint32_t payload =
                                ipc::encodeCalibPayload(m_threshFlatWp,
                                                        static_cast<uint16_t>(CM::FLAT_WP));
                            MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT,
                                         ipc::PulseCode::HAL_ADC_CALIBRATION, payload);

                            m_flatReferenceSent = true;
                            std::cout << "[Calib] Stufe 2 fertig: FLAT_WP = "
                                      << m_threshFlatWp
                                      << ". Erfasse Gesamtlaenge..." << std::endl;
                        }
                    }
                } else {
                    ++m_releaseConfirmCounter;

                    if (m_releaseConfirmCounter >= CALIBRATION_RELEASE_CONFIRM_COUNT) {
                        if (!m_flatReferenceSent && m_flatSampleCount > 0) {
                            m_threshFlatWp =
                                averageAsUInt16(m_flatSampleSum, m_flatSampleCount);
                            updateCalibrationThresholds(static_cast<uint16_t>(CM::FLAT_WP),
                                                        m_threshFlatWp);

                            const uint32_t flatPayload =
                                ipc::encodeCalibPayload(m_threshFlatWp,
                                                        static_cast<uint16_t>(CM::FLAT_WP));
                            MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT,
                                         ipc::PulseCode::HAL_ADC_CALIBRATION, flatPayload);
                            m_flatReferenceSent = true;
                        }

                        m_threshMinSamples = static_cast<uint16_t>(m_sampleCounter);
                        updateCalibrationThresholds(static_cast<uint16_t>(CM::MIN_SAMPLES),
                                                    m_threshMinSamples);

                        std::cout << "\n==================================================" << std::endl;
                        std::cout << "     >>> KALIBRIERUNG ERFOLGREICH BEENDET <<<     " << std::endl;
                        std::cout << "==================================================" << std::endl;
                        std::cout << " [Basis] EMPTY_BELT       : " << m_threshEmptyBelt << " ADC" << std::endl;
                        std::cout << " [Basis] FLAT_WP          : " << m_threshFlatWp << " ADC" << std::endl;
                        std::cout << " [Basis] TRIGGER-SCHWELLE : " << m_triggerValue << " ADC" << std::endl;
                        std::cout << " [Basis] RELEASE-SCHWELLE : " << m_releaseTriggerValue << " ADC" << std::endl;
                        std::cout << "--------------------------------------------------" << std::endl;
                        std::cout << " [Fenster] FLAT-ZONEN     : " << m_threshFlatMin << " bis " << m_threshFlatMax << " ADC" << std::endl;
                        std::cout << " [Fenster] TALL-ZONEN     : " << m_threshTallMin << " bis " << m_threshTallMax << " ADC" << std::endl;
                        std::cout << " [Fenster] DRILLED-ZONEN  : " << m_threshDrilledMin << " bis " << m_threshDrilledMax << " ADC" << std::endl;
                        std::cout << " [Fenster] MAX_HEIGHT     : < " << m_threshMaxHeight << " ADC (Fehler)" << std::endl;
                        std::cout << "--------------------------------------------------" << std::endl;
                        std::cout << " [Timing] MIN_SAMPLES     : " << m_threshMinSamples << " Samples" << std::endl;
                        std::cout << "==================================================\n" << std::endl;

                        const uint32_t payload =
                            ipc::encodeCalibPayload(m_threshMinSamples,
                                                    static_cast<uint16_t>(CM::MIN_SAMPLES));
                        MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT,
                                     ipc::PulseCode::HAL_ADC_CALIBRATION, payload);

                        sendHeightState(ipc::State::CLEARED);
                        m_nextWaitingForWpIsServiceTiming = true;
                        m_currentState = AnalyzerState::IDLE;
                        resetMeasurementData();
                        resetCalibrationData();
                    }
                }
                break;
            }
        }
        break;
    }
}

// MESSLAUF

void ProfileAnalyzer::beginMeasurement()
{
    m_currentState = AnalyzerState::MEASURING;
    std::cout << "[Analyzer] State -> MEASURING" << std::endl;
    m_transportPaused = false;
    m_sampleCounter = 0;
    m_flatCounter = 0;
    m_tallCounter = 0;
    m_drilledCounter = 0;
    m_unknownCounter = 0;
    m_measurementMinAdc = ADC_MAX_VALUE;
    m_measurementMaxAdc = 0;
    m_releaseConfirmCounter = 0;
    resetTallDrilledPattern();
    sendHeightState(ipc::State::BLOCKED);
}

void ProfileAnalyzer::addMeasurementSample(uint16_t currentRaw)
{
    ++m_sampleCounter;
    updateProfileCounters(currentRaw);
}

void ProfileAnalyzer::updateProfileCounters(uint16_t currentRaw)
{
    m_measurementMinAdc = std::min(m_measurementMinAdc, currentRaw);
    m_measurementMaxAdc = std::max(m_measurementMaxAdc, currentRaw);

    const bool isFlat = isInRange(currentRaw, m_threshFlatMin, m_threshFlatMax);
    const bool isTall = isInRange(currentRaw, m_threshTallMin, m_threshTallMax);
    const bool isDrilled = isInRange(currentRaw, m_threshDrilledMin, m_threshDrilledMax);

    const int matches = static_cast<int>(isFlat)
                      + static_cast<int>(isTall)
                      + static_cast<int>(isDrilled);

    if (matches != 1) {
        ++m_unknownCounter;
    } else if (isDrilled) {
        ++m_drilledCounter;
    } else if (isTall) {
        ++m_tallCounter;
    } else {
        ++m_flatCounter;
    }

    updateTallDrilledPattern(currentRaw);
}

void ProfileAnalyzer::updateTallDrilledPattern(uint16_t currentRaw)
{
    if (m_isTallDrilled) {
        return;
    }

    const bool isFlat = isInRange(currentRaw, m_threshFlatMin, m_threshFlatMax);
    const bool isTall = isInRange(currentRaw, m_threshTallMin, m_threshTallMax);
    const bool isDrilled = isInRange(currentRaw, m_threshDrilledMin, m_threshDrilledMax);
    const size_t confirmCount = tallDrilledConfirmCount();

    switch (m_tallDrilledPatternState) {
    case TallDrilledPatternState::SEARCH_TALL_BEFORE:
        if (isTall) {
            ++m_tallBeforeDrilledCounter;

            if (m_tallBeforeDrilledCounter >= confirmCount) {
                m_tallDrilledPatternState =
                    TallDrilledPatternState::SEARCH_DRILLED_AREA;
            }
        } else if (isFlat) {
            m_tallBeforeDrilledCounter = 0;
        }
        break;

    case TallDrilledPatternState::SEARCH_DRILLED_AREA:
        if (isDrilled) {
            ++m_drilledPatternCounter;

            if (m_drilledPatternCounter >= confirmCount) {
                m_tallDrilledPatternState =
                    TallDrilledPatternState::SEARCH_TALL_AFTER;
            }
        } else if (isFlat) {
            m_tallDrilledPatternState =
                TallDrilledPatternState::SEARCH_TALL_BEFORE;
            m_tallBeforeDrilledCounter = 0;
            m_drilledPatternCounter = 0;
        }
        break;

    case TallDrilledPatternState::SEARCH_TALL_AFTER:
        if (isTall) {
            ++m_tallAfterDrilledCounter;

            if (m_tallAfterDrilledCounter >= confirmCount) {
                m_isTallDrilled = true;
                m_tallDrilledPatternState =
                    TallDrilledPatternState::CONFIRMED;
            }
        } else if (isFlat) {
            m_tallDrilledPatternState =
                TallDrilledPatternState::SEARCH_TALL_BEFORE;
            m_tallBeforeDrilledCounter = 0;
            m_drilledPatternCounter = 0;
            m_tallAfterDrilledCounter = 0;
        }
        break;

    case TallDrilledPatternState::CONFIRMED:
        break;
    }
}

void ProfileAnalyzer::finishMeasurement()
{
    if (m_serviceTimingOnly) {
        std::cout << "[Analyzer] Service-Timing beendet -> Sende HEIGHT_CLEARED ohne Typklassifikation." << std::endl;
        sendHeightState(ipc::State::CLEARED);
        m_serviceTimingOnly = false;
        m_currentState = AnalyzerState::IDLE;
        std::cout << "[Analyzer] State -> IDLE" << std::endl;
        resetMeasurementData();
        return;
    }

    const size_t minSamples = minRequiredMeasurementSamples();
    const bool shortMeasurement = m_sampleCounter < minSamples;
    if (shortMeasurement) {
        std::cout << "[Analyzer] Messung zu kurz (" << m_sampleCounter
                  << " Samples, Minimum " << minSamples
                  << ") -> UNKNOWN." << std::endl;
        evaluationFinished(static_cast<uint16_t>(WpType::UNKNOWN));
    } else {
        const uint16_t finalWpType = determineFinalWorkpieceType();
        evaluationFinished(finalWpType);
    }

    m_currentState = AnalyzerState::WAITING_FOR_WP;
    std::cout << "[Analyzer] State -> WAITING_FOR_WP" << std::endl;
    resetMeasurementData();
    resetSmoothingBuffer();
    if (shortMeasurement) {
        m_waitForTunnelClear = true;
    }
}

// KALIBRIERUNG UND SCHWELLEN

void ProfileAnalyzer::updateCalibrationThresholds(uint16_t calib_mode, uint16_t calib_value)
{
    using CM = ipc::CalibMode;

    switch (static_cast<CM>(calib_mode)) {
    case CM::EMPTY_BELT:
        m_threshEmptyBelt = calib_value;
        m_triggerValue = safeSubtract(m_threshEmptyBelt, TRIGGER_OFFSET);
        m_releaseTriggerValue = safeSubtract(m_threshEmptyBelt, RELEASE_OFFSET);
        break;

    case CM::FLAT_WP:
        m_threshFlatWp = calib_value;
        recalculateTypeThresholds(calib_value);
        break;

    case CM::MIN_SAMPLES:
        m_threshMinSamples = calib_value == 0 ? DEFAULT_MIN_SAMPLES : calib_value;
        break;

    case CM::FLAT_MIN:
        m_threshFlatMin = calib_value;
        break;
    case CM::FLAT_MAX:
        m_threshFlatMax = calib_value;
        break;
    case CM::TALL_MIN:
        m_threshTallMin = calib_value;
        break;
    case CM::TALL_MAX:
        m_threshTallMax = calib_value;
        break;
    case CM::DRILLED_MIN:
        m_threshDrilledMin = calib_value;
        break;
    case CM::DRILLED_MAX:
        m_threshDrilledMax = calib_value;
        break;
    case CM::MAX_HEIGHT:
        m_threshMaxHeight = calib_value;
        break;

    default:
        break;
    }
}

void ProfileAnalyzer::recalculateTypeThresholds(uint16_t flatCenter)
{
    m_threshFlatMin = safeSubtract(flatCenter, FLAT_TOLERANCE);
    m_threshFlatMax = safeAdd(flatCenter, FLAT_TOLERANCE);

    const uint16_t tallCenter = safeSubtract(flatCenter, TALL_CENTER_OFFSET);
    m_threshTallMin = safeSubtract(tallCenter, TALL_TOLERANCE);
    m_threshTallMax = safeAdd(tallCenter, TALL_TOLERANCE);

    const uint16_t drilledCenter = safeAdd(flatCenter, DRILLED_CENTER_OFFSET);
    m_threshDrilledMin = safeSubtract(drilledCenter, DRILLED_TOLERANCE);
    m_threshDrilledMax = safeAdd(drilledCenter, DRILLED_TOLERANCE);

    m_threshMaxHeight = safeSubtract(m_threshTallMin, MAX_HEIGHT_MARGIN);
}

// PROFILAUSWERTUNG

uint16_t ProfileAnalyzer::determineFinalWorkpieceType()
{
    using WT = WpType;

    if (m_sampleCounter < minRequiredMeasurementSamples()) {
        return static_cast<uint16_t>(WT::UNKNOWN);
    }

    const size_t mainClassThreshold = minSamplesForMainClass();
    const size_t drilledClassThreshold = minSamplesForDrilledClass();
    const bool flatRelevant = m_flatCounter >= mainClassThreshold;
    const bool tallRelevant = m_tallCounter >= mainClassThreshold;
    const bool drilledRelevant = m_drilledCounter >= drilledClassThreshold;
    const bool tallEvidenceForDrilled =
        m_tallCounter >= drilledClassThreshold;
    const uint32_t strongestKnownCounter =
        std::max(m_flatCounter, std::max(m_tallCounter, m_drilledCounter));
    const bool knownEvidenceExists =
        flatRelevant || tallRelevant || drilledRelevant;
    const bool unknownDominates =
        m_unknownCounter >= drilledClassThreshold &&
        (m_unknownCounter * 100) >
            (m_sampleCounter * UNKNOWN_DOMINANCE_PERCENT) &&
        (!knownEvidenceExists ||
         m_unknownCounter > (strongestKnownCounter * 2));

    std::cout << "[Scan] Finale Counter: flat=" << m_flatCounter
              << " tall=" << m_tallCounter
              << " drilled=" << m_drilledCounter
              << " unknown=" << m_unknownCounter
              << " samples=" << m_sampleCounter
              << " mainClassThreshold=" << mainClassThreshold
              << " drilledClassThreshold=" << drilledClassThreshold
              << " tallDrilledPattern=" << (m_isTallDrilled ? "true" : "false")
              << " min=" << m_measurementMinAdc
              << " max=" << m_measurementMaxAdc
              << std::endl;

    if (unknownDominates) {
        return static_cast<uint16_t>(WT::UNKNOWN);
    }

    if (flatRelevant && tallEvidenceForDrilled) {
        return static_cast<uint16_t>(WT::UNKNOWN);
    }

    if (!flatRelevant &&
        drilledRelevant &&
        (m_isTallDrilled || tallRelevant || tallEvidenceForDrilled)) {
        return static_cast<uint16_t>(WT::TALL_DRILLED);
    }

    if (tallRelevant) {
        return static_cast<uint16_t>(WT::TALL);
    }

    if (flatRelevant) {
        return static_cast<uint16_t>(WT::FLAT);
    }

    return static_cast<uint16_t>(WT::UNKNOWN);
}

// HILFSMETHODEN

void ProfileAnalyzer::evaluationFinished(uint16_t finalTypeOrValue)
{
    std::cout << "\n[TEST-MONITOR] >>> ERKANNTES WERKSTUECK: "
              << typeName(finalTypeOrValue) << " <<<\n" << std::endl;
    const uint32_t typePayload =
        ipc::encodeHalPayload(ipc::SensorID::HEIGHT, finalTypeOrValue);
    MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT,
                 ipc::PulseCode::HAL_INPUT, typePayload);
}

bool ProfileAnalyzer::handleHeightError(uint16_t currentRaw)
{
    if (m_threshMaxHeight == 0) {
        m_heightErrorActive = false;
        return false;
    }

    const bool heightError = currentRaw < m_threshMaxHeight;
    if (!heightError) {
        m_heightErrorActive = false;
        return false;
    }

    if (!m_heightErrorActive) {
        std::cerr << "[Analyzer] CRITICAL: Werkstueck zu hoch! ADC: "
                  << currentRaw << std::endl;
        sendHeightState(ipc::State::HIGH_ERROR);
        m_heightErrorActive = true;
    }

    return true;
}

void ProfileAnalyzer::sendHeightState(uint16_t state)
{
    const uint32_t payload = ipc::encodeHalPayload(ipc::SensorID::HEIGHT, state);
    MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT,
                 ipc::PulseCode::HAL_INPUT, payload);
}

bool ProfileAnalyzer::isCalibrated() const
{
    return m_triggerValue > 0 &&
           m_releaseTriggerValue > 0 &&
           m_threshFlatMin < m_threshFlatMax &&
           m_threshTallMin < m_threshTallMax &&
           m_threshDrilledMin < m_threshDrilledMax &&
           m_threshMinSamples > 0;
}

bool ProfileAnalyzer::isInRange(uint16_t value, uint16_t minValue, uint16_t maxValue) const
{
    if (minValue > maxValue) {
        std::swap(minValue, maxValue);
    }

    return value >= minValue && value <= maxValue;
}

size_t ProfileAnalyzer::minRequiredMeasurementSamples() const
{
    const size_t calibratedSampleCount =
        m_threshMinSamples > 0 ? m_threshMinSamples : DEFAULT_MIN_SAMPLES;

    return std::max<size_t>(1,
                            percentCeil(calibratedSampleCount,
                                        MIN_MEASUREMENT_PERCENT));
}

size_t ProfileAnalyzer::minSamplesForMainClass() const
{
    const size_t sampleBasis =
        m_sampleCounter > 0 ? m_sampleCounter : minRequiredMeasurementSamples();

    return std::max<size_t>(2,
                            percentCeil(sampleBasis, MAIN_CLASS_PERCENT));
}

size_t ProfileAnalyzer::minSamplesForDrilledClass() const
{
    const size_t sampleBasis =
        m_sampleCounter > 0 ? m_sampleCounter : minRequiredMeasurementSamples();

    return std::max<size_t>(2,
                            percentCeil(sampleBasis, DRILLED_CLASS_PERCENT));
}

size_t ProfileAnalyzer::tallDrilledConfirmCount() const
{
    return std::max<size_t>(3, minSamplesForDrilledClass());
}

size_t ProfileAnalyzer::runningTriggerConfirmCount() const
{
    if (m_threshMinSamples == 0) {
        return 3;
    }

    return std::max<size_t>(2, std::min<size_t>(5, m_threshMinSamples / 6));
}

size_t ProfileAnalyzer::runningReleaseConfirmCount() const
{
    return runningTriggerConfirmCount();
}

const char* ProfileAnalyzer::typeName(uint16_t type) const
{
    switch (static_cast<WpType>(type)) {
    case WpType::FLAT:
        return "FLAT (Flach)";
    case WpType::TALL:
        return "TALL (Hoch)";
    case WpType::TALL_DRILLED:
        return "TALL_DRILLED (Gebohrt)";
    case WpType::TALL_DRILLED_METAL:
        return "TALL_DRILLED_METAL";
    case WpType::TALL_METAL:
        return "TALL_METAL";
    default:
        return "UNKNOWN";
    }
}
