/**
 * @file ProfileAnalyzer.hpp
 * @brief Logische Zustandsschaltung und Auswertung der ADC-Höhenprofile (Optimierte Segmentgröße).
 */

#ifndef PROFILE_ANALYZER_HPP
#define PROFILE_ANALYZER_HPP

#include <cstdint>
#include <vector>

class ProfileAnalyzer {
private:
    int m_coid;

    enum class AnalyzerState {
        IDLE,
        WAITING_FOR_WP,
        MEASURING,
        CALIBRATION
    };
    AnalyzerState m_currentState;

    enum class CalibStep {
        MEASURE_BELT,
        WAIT_FOR_WP,
        WAIT_FOR_LEAVE
    };
    CalibStep m_currentCalibStep;

    // Kalibrierungswerte als dedizierte Variablen
    uint16_t m_threshEmptyBelt;
    uint16_t m_threshFlatWp;
    uint16_t m_threshMinSamples;

    // Dynamische Trigger-Schwelldifferenz (Berechnet in Stufe 1)
    uint16_t m_heightBlocked;

    // Vorberechnete feste Segmentgröße für den Messlauf
    size_t m_samplesPerSegment;

    // Berechnete Toleranzgrenzen für die Live-Erkennung
    uint16_t m_threshFlatMin;
    uint16_t m_threshFlatMax;
    uint16_t m_threshTallMin;
    uint16_t m_threshTallMax;
    uint16_t m_threshDrilledMin;
    uint16_t m_threshDrilledMax;
    uint16_t m_threshMaxHeight;

    uint32_t m_sampleCounter;

    // Block-Sammel-Puffer für die erste Glättung (10 Werte)
    static constexpr size_t SMOOTHING_SAMPLE_COUNT = 10;
    uint32_t m_rawBuffer[SMOOTHING_SAMPLE_COUNT];
    size_t m_fillCount;

    // LIVE-SEGMENTIERUNG (12 Klassen)
    static constexpr size_t NUM_PROFILE_CLASSES = 12;
    uint16_t m_segmentTypes[NUM_PROFILE_CLASSES];
    size_t m_currentSegmentIndex;

    // Dynamischer Zwischenpuffer für genau ein Segment
    std::vector<uint16_t> m_currentSegmentBuffer;

    uint16_t determineSegmentType(const std::vector<uint16_t>& segmentSamples);
    uint16_t determineFinalWorkpieceType();
    uint32_t calculateBlockAverage();
    void recalculateTypeThresholds(uint16_t flatCenter);
    void evaluationFinished(uint16_t finalTypeOrValue);

public:
    ProfileAnalyzer();
    ~ProfileAnalyzer() = default;

    void setConnectionId(int coid);
    void processSample(uint32_t adcValue);

    void setAdcIdelState();
    void setAdcWaitigForWpState();
    void setAdcCalibrationsState();
    void updateCalibrationThresholds(uint16_t calib_mode, uint16_t calib_value);
};

#endif // PROFILE_ANALYZER_HPP