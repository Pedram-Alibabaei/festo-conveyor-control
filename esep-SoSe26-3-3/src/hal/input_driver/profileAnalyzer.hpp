/**
 * @file ProfileAnalyzer.hpp
 * @brief Logische Zustandsschaltung und Auswertung der ADC-Hoehenprofile.
 */

#ifndef PROFILE_ANALYZER_HPP
#define PROFILE_ANALYZER_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

class ProfileAnalyzer {
private:
    int m_coid = -1;

    enum class AnalyzerState {
        IDLE,
        WAITING_FOR_WP,
        MEASURING,
        MEASURING_PAUSED,
        CALIBRATION
    };
    AnalyzerState m_currentState = AnalyzerState::IDLE;

    enum class CalibStep {
        MEASURE_BELT,
        WAIT_FOR_WP,
        WAIT_FOR_LEAVE
    };
    CalibStep m_currentCalibStep = CalibStep::MEASURE_BELT;

    enum class TallDrilledPatternState {
        SEARCH_TALL_BEFORE,
        SEARCH_DRILLED_AREA,
        SEARCH_TALL_AFTER,
        CONFIRMED
    };
    TallDrilledPatternState m_tallDrilledPatternState =
        TallDrilledPatternState::SEARCH_TALL_BEFORE;

    // Kalibrierungswerte als dedizierte Variablen
    uint16_t m_threshEmptyBelt = 0;
    uint16_t m_threshFlatWp = 0;
    uint16_t m_threshMinSamples = 0;

    // Dynamische Trigger-/Release-Schwellen aus der Empty-Belt-Kalibrierung
    uint16_t m_triggerValue = 0;
    uint16_t m_releaseTriggerValue = 0;

    // Berechnete Toleranzgrenzen für die Live-Erkennung
    uint16_t m_threshFlatMin = 0;
    uint16_t m_threshFlatMax = 0;
    uint16_t m_threshTallMin = 0;
    uint16_t m_threshTallMax = 0;
    uint16_t m_threshDrilledMin = 0;
    uint16_t m_threshDrilledMax = 0;
    uint16_t m_threshMaxHeight = 0;

    uint32_t m_sampleCounter = 0;
    uint32_t m_flatCounter = 0;
    uint32_t m_tallCounter = 0;
    uint32_t m_drilledCounter = 0;
    uint32_t m_unknownCounter = 0;
    bool m_transportPaused = false;
    bool m_heightErrorActive = false;
    bool m_serviceTimingOnly = false;
    bool m_waitForTunnelClear = false;
    bool m_nextWaitingForWpIsServiceTiming = false;
    bool m_isTallDrilled = false;
    uint32_t m_tallBeforeDrilledCounter = 0;
    uint32_t m_drilledPatternCounter = 0;
    uint32_t m_tallAfterDrilledCounter = 0;
    uint16_t m_measurementMinAdc = 0;
    uint16_t m_measurementMaxAdc = 0;
    uint32_t m_beltSampleSum = 0;
    uint32_t m_flatSampleSum = 0;

    size_t m_flatSampleCount = 0;
    size_t m_triggerConfirmCounter = 0;
    size_t m_releaseConfirmCounter = 0;
    bool m_flatReferenceSent = false;

    // Block-Sammel-Puffer für die erste Glättung (10 Werte)
    static constexpr size_t SMOOTHING_SAMPLE_COUNT = 10;
    uint32_t m_rawBuffer[SMOOTHING_SAMPLE_COUNT] = {};
    size_t m_fillCount = 0;

    std::vector<uint16_t> m_triggerCandidateBuffer;

    uint16_t determineFinalWorkpieceType();
    uint32_t calculateBlockAverage();
    void recalculateTypeThresholds(uint16_t flatCenter);
    void evaluationFinished(uint16_t finalTypeOrValue);
    void resetSmoothingBuffer();
    void resetMeasurementData();
    void resetCalibrationData();
    void resetTallDrilledPattern();
    void beginMeasurement();
    void addMeasurementSample(uint16_t currentRaw);
    void updateProfileCounters(uint16_t currentRaw);
    void updateTallDrilledPattern(uint16_t currentRaw);
    void finishMeasurement();
    bool handleHeightError(uint16_t currentRaw);
    void sendHeightState(uint16_t state);
    bool isCalibrated() const;
    bool isInRange(uint16_t value, uint16_t minValue, uint16_t maxValue) const;
    size_t minRequiredMeasurementSamples() const;
    size_t minSamplesForMainClass() const;
    size_t minSamplesForDrilledClass() const;
    size_t tallDrilledConfirmCount() const;
    size_t runningTriggerConfirmCount() const;
    size_t runningReleaseConfirmCount() const;
    const char* typeName(uint16_t type) const;

public:
    ProfileAnalyzer();
    ~ProfileAnalyzer() = default;

    void setConnectionId(int coid);
    void processSample(uint32_t adcValue);

    void setAdcIdelState();
    void setAdcWaitigForWpState();
    void setAdcCalibrationsState();
    /** Service-Kalibrierung: nur HEIGHT_BLOCKED/CLEARED fuer Tick-Messung, ohne Typklassifikation. */
    void setServiceTimingMeasureState();
    void setTransportPaused(bool paused);
    void updateCalibrationThresholds(uint16_t calib_mode, uint16_t calib_value);
};

#endif // PROFILE_ANALYZER_HPP
