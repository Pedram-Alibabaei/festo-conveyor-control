/**
 * @file ProfileAnalyzer.hpp
 * @brief Logische Zustandsschaltung und Auswertung der ADC-Höhenprofile.
 */

#ifndef PROFILE_ANALYZER_HPP
#define PROFILE_ANALYZER_HPP

#include <cstdint>
#include <map>
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

    // Interne Schritte für die sequentielle Kalibrierung
    enum class CalibStep {
        MEASURE_BELT,       // Stufe 1: Leeres Band einmessen
        WAIT_FOR_WP,        // Stufe 2: Auf Werkstück warten & Peak einmessen
        WAIT_FOR_LEAVE      // Stufe 3: Zählen, bis Werkstück Band verlässt (MIN_SAMPLES)
    };
    CalibStep m_currentCalibStep;

    std::map<uint16_t, uint16_t> m_thresholds;
    uint16_t m_maxHeightInCurrentProfile;
    uint32_t m_sampleCounter;
    uint16_t m_tempBeltValue; // Temporärer Speicher für die Band-Erkennung während des Einlernens

    // 1. Block-Sammel-Puffer für die erste Glättung (10 Werte)
    static constexpr size_t SMOOTHING_SAMPLE_COUNT = 10;
    uint32_t m_rawBuffer[SMOOTHING_SAMPLE_COUNT];
    size_t m_fillCount;

    // ========================================================================
    // LIVE-SEGMENTIERUNG (12 Klassen)
    // ========================================================================
    static constexpr size_t NUM_PROFILE_CLASSES = 12;

    // Speicher für die ermittelten Typen der 12 Segmente
    uint16_t m_segmentTypes[NUM_PROFILE_CLASSES];
    size_t m_currentSegmentIndex; // Welches Segment befüllen wir gerade (0 bis 11)?

    // Dynamischer Zwischenpuffer für genau ein Segment (Größe = MIN_SAMPLES / 12)
    std::vector<uint16_t> m_currentSegmentBuffer;

    /**
     * @brief Bestimmt den Zustand/Typ eines einzelnen Segments anhand der gesammelten Samples.
     * @param segmentSamples Der Puffer mit den Rohwerten dieses Segments.
     * @return uint16_t Der ermittelte Typ für dieses Segment.
     */
    uint16_t determineSegmentType(const std::vector<uint16_t>& segmentSamples);

    /**
     * @brief Analysiert die fertige Kette aus 12 Segment-Typen und bestimmt den Werkstück-Gesamttyp.
     */
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