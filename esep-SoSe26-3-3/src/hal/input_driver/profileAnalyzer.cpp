/**
 * @file ProfileAnalyzer.cpp
 * @brief Logische Zustandsschaltung und Auswertung der ADC-Höhenprofile.
 */

#include "ProfileAnalyzer.hpp"
#include "shared/ipc_definitions.hpp"
#include "shared/types.hpp"
#include <iostream>

extern "C" {
#include <sys/neutrino.h>
}

// ============================================================================
// KONSTRUKTOR & INITIALISIERUNG
// ============================================================================

ProfileAnalyzer::ProfileAnalyzer()
    : m_coid(-1),
      m_currentState(AnalyzerState::IDLE),
      m_currentCalibStep(CalibStep::MEASURE_BELT),
      m_threshEmptyBelt(-1),
      m_threshFlatWp(-1),
      m_threshMinSamples(-1),
      m_heightBlocked(-1),       // Sicherer Standardwert vor der ersten Messung
      m_samplesPerSegment(-1),     // Standard-Fallback (120 / 12 = 10)
      m_threshFlatMin(-1), m_threshFlatMax(-1),
      m_threshTallMin(-1), m_threshTallMax(-1),
      m_threshDrilledMin(-1), m_threshDrilledMax(-1),
      m_threshMaxHeight(-1),
      m_sampleCounter(-1),
      m_fillCount(-1),
      m_currentSegmentIndex(-1) {

    for (size_t i = 0; i < SMOOTHING_SAMPLE_COUNT; ++i) {
        m_rawBuffer[i] = 0;
    }
    for (size_t i = 0; i < NUM_PROFILE_CLASSES; ++i) {
        m_segmentTypes[i] = static_cast<uint16_t>(WpType::UNKNOWN);
    }
}

void ProfileAnalyzer::setConnectionId(int coid) { m_coid = coid; }
void ProfileAnalyzer::setAdcIdelState() { m_currentState = AnalyzerState::IDLE; }
void ProfileAnalyzer::setAdcWaitigForWpState() { m_currentState = AnalyzerState::WAITING_FOR_WP; }

void ProfileAnalyzer::setAdcCalibrationsState() {
    m_currentState = AnalyzerState::CALIBRATION;
    m_currentCalibStep = CalibStep::MEASURE_BELT;
    m_sampleCounter = 0;
}

uint32_t ProfileAnalyzer::calculateBlockAverage() {
    uint64_t sum = 0;
    for (size_t i = 0; i < SMOOTHING_SAMPLE_COUNT; ++i) {
        sum += m_rawBuffer[i];
    }
    return static_cast<uint32_t>(sum / SMOOTHING_SAMPLE_COUNT);
}

// ============================================================================
// SAMPLER VERARBEITUNG
// ============================================================================

void ProfileAnalyzer::processSample(uint32_t adcValue) {
    if (m_coid == -1) return;

    m_rawBuffer[m_fillCount] = adcValue;
    m_fillCount++;

    if (m_fillCount < SMOOTHING_SAMPLE_COUNT) return;

    uint32_t smoothedValue = calculateBlockAverage();
    m_fillCount = 0;

    uint16_t currentRaw = static_cast<uint16_t>(smoothedValue & 0xFFF);

    switch (m_currentState) {

        case AnalyzerState::IDLE:
            break;

        case AnalyzerState::WAITING_FOR_WP:

            {
                if (currentRaw < m_heightBlocked) {
                    std::cerr << "[Analyzer] currentRaw < m_heightBlocked " << currentRaw << std::endl;
                    m_sampleCounter = 1;
                    m_currentSegmentIndex = 0;
                    m_currentSegmentBuffer.clear();

                    for (size_t i = 0; i < NUM_PROFILE_CLASSES; ++i) {
                        m_segmentTypes[i] = static_cast<uint16_t>(WpType::UNKNOWN);
                    }
                    // ========================================================================
                    // HEIGHT_BLOCKED Event an den Dispatcher senden
                    // ========================================================================
                    std::cout << "[Analyzer] Werkstueck an HS erkannt -> Sende HEIGHT_BLOCKED" << std::endl;

                    // Wir nutzen das HAL-Input-Payload-Encoding (Sensor-ID + Zustand)
                    uint32_t payload = ipc::encodeHalPayload(ipc::SensorID::HEIGHT, ipc::State::BLOCKED);
                    MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_INPUT, payload);
                    // ========================================================================
                    m_currentState = AnalyzerState::MEASURING;
                }
            }
            break;

        case AnalyzerState::MEASURING:
            {
                // HÖHEN-FEHLER CHECK
                if (currentRaw < m_threshMaxHeight) {
                    std::cerr << "[Analyzer] CRITICAL: Werkstueck zu hoch! ADC: " << currentRaw << std::endl;
                    uint32_t payload = ipc::encodeHalPayload(ipc::SensorID::HEIGHT, ipc::State::HIGH_ERROR);
                    MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_INPUT, payload);
                    m_currentState = AnalyzerState::IDLE;
                    break;
                }

                // NORMALER SCAN
                if (currentRaw < m_heightBlocked) {
                    m_sampleCounter++;

                    m_currentSegmentBuffer.push_back(currentRaw);

                    // Wenn ein Segment voll ist, werten wir es aus und loggen das Ergebnis
                    if (m_currentSegmentBuffer.size() >= m_samplesPerSegment && m_currentSegmentIndex < NUM_PROFILE_CLASSES) {

                        // 1. Segment-Typ bestimmen
                        uint16_t detectedType = determineSegmentType(m_currentSegmentBuffer);
                        m_segmentTypes[m_currentSegmentIndex] = detectedType;

                        // ========================================================================
                        // LIVE-LOGGING FÜR JEDES SEGMENT
                        // ========================================================================
                        std::string segmentName = "?";
                        switch (static_cast<WpType>(detectedType)) {
                            case WpType::FLAT:         segmentName = "___Flach"; break; // Flach / Tief
                            case WpType::TALL:         segmentName = "███Hoch"; break; // Hoch
                            case WpType::TALL_DRILLED: segmentName = "░░░Bohrung"; break; // Bohrung (Mittelwert)
                            default:                   segmentName = "???"; break;
                        }

                        std::cout << "[Scan] Segment [" << m_currentSegmentIndex + 1 << "/12] erkannt als: "
                                  << segmentName << " (ID: " << detectedType << ")" << std::endl;
                        // ========================================================================

                        m_currentSegmentIndex++;
                        m_currentSegmentBuffer.clear();
                    }
                }
                else {
                    // WERKSTÜCK HAT SENSOR VERLASSEN
                    if (!m_currentSegmentBuffer.empty() && m_currentSegmentIndex < NUM_PROFILE_CLASSES) {
                        m_segmentTypes[m_currentSegmentIndex] = determineSegmentType(m_currentSegmentBuffer);
                    }

                    if (m_sampleCounter >= (m_threshMinSamples * 0.7)) {
                        uint16_t finalWpType = determineFinalWorkpieceType();
                        evaluationFinished(finalWpType);
                    } else {
                        std::cout << "[Analyzer] Messung verworfen (Rauschen)." << std::endl;
                    }
                    m_currentState = AnalyzerState::IDLE;
                }
            }
            break;

        case AnalyzerState::CALIBRATION:
            {
                using CM = ipc::CalibMode;

                switch (m_currentCalibStep) {

                    case CalibStep::MEASURE_BELT:
                        m_sampleCounter++;
                        if (m_sampleCounter == 1) {
                            m_threshEmptyBelt = currentRaw;
                        } else {
                            m_threshEmptyBelt = (m_threshEmptyBelt + currentRaw) / 2;
                        }

                        if (m_sampleCounter >= 50) {
                        	updateCalibrationThresholds(static_cast<uint16_t>(ipc::CalibMode::EMPTY_BELT), m_threshEmptyBelt);

                            uint32_t payload = ipc::encodeCalibPayload(m_threshEmptyBelt, static_cast<uint16_t>(CM::EMPTY_BELT));
                            MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_ADC_CALIBRATION, payload);

                            std::cout << "[Calib] Stufe 1 fertig: EMPTY_BELT = " << m_threshEmptyBelt
                                      << " | m_heightBlocked = " << m_heightBlocked << ". Warte auf WP..." << std::endl;

                            m_currentCalibStep = CalibStep::WAIT_FOR_WP;
                            m_sampleCounter = 0;
                        }
                        break;

                    case CalibStep::WAIT_FOR_WP:
                        if (currentRaw <= m_heightBlocked) {
                            m_sampleCounter++;

                            // Erhöht auf 150 Samples für eine stabil gemittelte Werkstückplatte
                            if (m_sampleCounter == 150) {
                                m_threshFlatWp = currentRaw;
                                updateCalibrationThresholds(static_cast<uint16_t>(ipc::CalibMode::FLAT_WP), m_threshFlatWp);
                                uint32_t payload = ipc::encodeCalibPayload(m_threshFlatWp, static_cast<uint16_t>(CM::FLAT_WP));
                                MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_ADC_CALIBRATION, payload);

                                std::cout << "[Calib] Stufe 2 fertig: FLAT_WP = " << m_threshFlatWp << ". Erfasse Gesamtlänge..." << std::endl;

                                m_currentCalibStep = CalibStep::WAIT_FOR_LEAVE;
                            }
                        }
                        break;

                    case CalibStep::WAIT_FOR_LEAVE:
                        m_sampleCounter++;

                        // Hysterese-Austritt: Verlässt das Werkstück die m_heightBlocked-Schwelle wieder nach oben?
                        if (currentRaw >= (m_heightBlocked + 20)) {
                            m_threshMinSamples = static_cast<uint16_t>(m_sampleCounter);
                            updateCalibrationThresholds(static_cast<uint16_t>(ipc::CalibMode::MIN_SAMPLES), m_threshMinSamples);
                            // Monitorausgabe aller ermittelten Variablen zur Überprüfung an der Anlage
                            std::cout << "\n==================================================" << std::endl;
                            std::cout << "     >>> KALIBRIERUNG ERFOLGREICH BEENDET <<<     " << std::endl;
                            std::cout << "==================================================" << std::endl;
                            std::cout << " [Basis] EMPTY_BELT       : " << m_threshEmptyBelt << " ADC" << std::endl;
                            std::cout << " [Basis] FLAT_WP          : " << m_threshFlatWp << " ADC" << std::endl;
                            std::cout << " [Basis] TRIGGER-SCHWELLE : " << m_heightBlocked << " ADC" << std::endl;
                            std::cout << "--------------------------------------------------" << std::endl;
                            std::cout << " [Fenster] FLAT-ZONEN     : " << m_threshFlatMin << " bis " << m_threshFlatMax << " ADC" << std::endl;
                            std::cout << " [Fenster] TALL-ZONEN     : " << m_threshTallMin << " bis " << m_threshTallMax << " ADC" << std::endl;
                            std::cout << " [Fenster] DRILLED-ZONEN  : " << m_threshDrilledMin << " bis " << m_threshDrilledMax << " ADC" << std::endl;
                            std::cout << " [Fenster] MAX_HEIGHT     : < " << m_threshMaxHeight << " ADC (Fehler)" << std::endl;
                            std::cout << "--------------------------------------------------" << std::endl;
                            std::cout << " [Timing] MIN_SAMPLES     : " << m_threshMinSamples << " Samples" << std::endl;
                            std::cout << " [Timing] SEGMENT-GRÖSSE  : " << m_samplesPerSegment << " Samples pro Klasse" << std::endl;
                            std::cout << "==================================================\n" << std::endl;

                            uint32_t payload = ipc::encodeCalibPayload(m_threshMinSamples, static_cast<uint16_t>(CM::MIN_SAMPLES));
                            MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_ADC_CALIBRATION, payload);

                            m_currentState = AnalyzerState::IDLE;
                        }
                        break;
                }
            }
            break;
    }
}

// ============================================================================
// RESTLICHE METHODEN (KORRIGIERTE MATHEMATIK)
// ============================================================================

void ProfileAnalyzer::updateCalibrationThresholds(uint16_t calib_mode, uint16_t calib_value) {
    using CM = ipc::CalibMode;
    switch (static_cast<CM>(calib_mode)) {
        case CM::EMPTY_BELT:
            m_threshEmptyBelt = calib_value;
            m_heightBlocked = m_threshEmptyBelt - 100; // Synchronisieren!
            break;
        case CM::FLAT_WP:
            m_threshFlatWp = calib_value;
            recalculateTypeThresholds(calib_value);
            break;
        case CM::MIN_SAMPLES:
            m_threshMinSamples = calib_value;
            m_samplesPerSegment =  calib_value / NUM_PROFILE_CLASSES;
            break;

        default: break;
    }
}

void ProfileAnalyzer::recalculateTypeThresholds(uint16_t flatCenter) {
    // Toleranzbereich um die flache Platte (z.B. +/- 25 Ticks)
    m_threshFlatMin    = flatCenter - 40;
    m_threshFlatMax    = flatCenter + 40;

    // KORREKTUR: Echtes Toleranzfenster für hohe Werkstücke erzeugen (Invertiert!)
    m_threshTallMin    = flatCenter - 270;
    m_threshTallMax    = flatCenter - 220;

    // KORREKTUR: Bohrungen liegen tiefer im Werkstück -> ADC-Werte steigen wieder an!
    // Daher MUSS hier MINUS gerechnet werden, nicht plus, um das Niveau der Bohrung zu treffen.
    m_threshDrilledMin = flatCenter + 620;
    m_threshDrilledMax = flatCenter + 900;

    // Maximal zulässige Höhe (Sicherheits-Fehlerschwelle)
    m_threshMaxHeight  = m_threshTallMax - 100;
}

uint16_t ProfileAnalyzer::determineSegmentType(const std::vector<uint16_t>& segmentSamples) {
    if (segmentSamples.empty()) return static_cast<uint16_t>(WpType::UNKNOWN);
    uint16_t segmentPeak = 4095;
    for (uint16_t val : segmentSamples) {
        if (val < segmentPeak) segmentPeak = val;
    }
    std::cout << "[Calib] segmentPeak = " << segmentPeak << "." << std::endl;
    if (segmentPeak >= m_threshDrilledMin && segmentPeak <= m_threshDrilledMax) return static_cast<uint16_t>(WpType::TALL_DRILLED);
    if (segmentPeak >= m_threshTallMin && segmentPeak <= m_threshTallMax) return static_cast<uint16_t>(WpType::TALL);
    if (segmentPeak >= m_threshFlatMin && segmentPeak <= m_threshFlatMax) return static_cast<uint16_t>(WpType::FLAT);
    return static_cast<uint16_t>(WpType::UNKNOWN);
}

uint16_t ProfileAnalyzer::determineFinalWorkpieceType() {
    using WT = WpType;
    bool allTall = true; bool allFlat = true;
    for (size_t i = 0; i < NUM_PROFILE_CLASSES; ++i) {
        if (m_segmentTypes[i] != static_cast<uint16_t>(WT::TALL)) allTall = false;
        if (m_segmentTypes[i] != static_cast<uint16_t>(WT::FLAT)) allFlat = false;
    }
    if (allTall) return static_cast<uint16_t>(WT::TALL);
    if (allFlat) return static_cast<uint16_t>(WT::FLAT);

    bool outerIsTall = (m_segmentTypes[0] == static_cast<uint16_t>(WT::TALL)) &&
                       (m_segmentTypes[1] == static_cast<uint16_t>(WT::TALL)) &&
                       (m_segmentTypes[9] == static_cast<uint16_t>(WT::TALL)) &&
                       (m_segmentTypes[10] == static_cast<uint16_t>(WT::TALL));

    bool centerIsDrilled = (m_segmentTypes[5] == static_cast<uint16_t>(WT::TALL_DRILLED)) &&
                           (m_segmentTypes[6] == static_cast<uint16_t>(WT::TALL_DRILLED));

    if (outerIsTall && centerIsDrilled) return static_cast<uint16_t>(WT::TALL_DRILLED);
    return static_cast<uint16_t>(WT::UNKNOWN);
}

void ProfileAnalyzer::evaluationFinished(uint16_t finalTypeOrValue) {
    std::string typeName = "UNKNOWN";
    switch (static_cast<WpType>(finalTypeOrValue)) {
        case WpType::FLAT:         typeName = "FLAT (Flach)"; break;
        case WpType::TALL:         typeName = "TALL (Hoch)"; break;
        case WpType::TALL_DRILLED: typeName = "TALL_DRILLED (Gebohrt)"; break;
        default:                   typeName = "UNKNOWN / INVALID"; break;
    }
    std::cout << "\n[TEST-MONITOR] >>> ERKANNTES WERKSTUECK: " << typeName << " <<<\n" << std::endl;

    //  Sende die Freigabe der Lichtschranke (landet im 'else if (sensor_state == ipc::State::CLEARED)'-Zweig)
    uint32_t clearPayload = ipc::encodeHalPayload(ipc::SensorID::HEIGHT, ipc::State::CLEARED);
    MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_INPUT, clearPayload);

    //  Sende den erkannten Typ (landet im 'else'-Zweig des Dispatchers)
    uint32_t typePayload = ipc::encodeHalPayload(ipc::SensorID::HEIGHT, finalTypeOrValue);
    MsgSendPulse(m_coid, SIGEV_PULSE_PRIO_INHERIT, ipc::PulseCode::HAL_INPUT, typePayload);

}
