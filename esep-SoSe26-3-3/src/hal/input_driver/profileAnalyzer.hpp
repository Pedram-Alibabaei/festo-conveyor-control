/**
 * @file ProfileAnalyzer.hpp
 * @brief Logische Zustandsschaltung und Auswertung der ADC-Höhenprofile.
 */

#ifndef PROFILE_ANALYZER_HPP
#define PROFILE_ANALYZER_HPP

#include <cstdint>
#include <map>

class ProfileAnalyzer {
private:
    // Die vom Dispatcher übergebene COID, um Ergebnisse autonom zu verschicken
    int m_coid;

    // Interne Zustandsmaschine für die Werkstück-Erkennung am ADC
    enum class AnalyzerState {
        IDLE,               // Band ist leer, keine Messung
        WAITING_FOR_WP,     // Bereit für das nächste Werkstück
        MEASURING,          // Werkstück befindet sich unter dem Sensor
        CALIBRATION         // Live-Kalibrierung läuft (Einlernen)
    };

    AnalyzerState m_currentState;

    // Speicher für die geladenen oder eingelernten Kalibrierungsschwellenwerte
    // Key: ipc::CalibMode (z.B. EMPTY_BELT, FLAT_WP) -> Value: ADC-Rohwert
    std::map<uint16_t, uint16_t> m_thresholds;

    // Interne Variablen zur Profilerfassung während einer Messung
    uint16_t m_maxHeightInCurrentProfile;
    uint32_t m_sampleCounter;

    /**
     * @brief Hilfsmethode, um das fertig gemessene Profil zu analysieren
     * und das Ergebnis als Puls an den Dispatcher zu senden.
     */
    void evaluationFinished(uint16_t finalTypeOrValue);

public:
    /**
     * @brief Konstruktor. Initialisiert den Analyzer im IDLE-Zustand.
     */
    ProfileAnalyzer();

    ~ProfileAnalyzer() = default;

    /**
     * @brief Setzt die Connection-ID (Kanal) zum Dispatcher.
     * Wird einmalig von HalSensors::init() aufgerufen.
     */
    void setConnectionId(int coid);

    /**
     * @brief Verarbeitet ein einzelnes ADC-Sample aus der Interrupt-Service-Routine.
     * @param adcValue Der rohe 32-Bit Analogwert direkt von der Hardware.
     */
    void processSample(uint32_t adcValue);

    // ========================================================================
    // LOGISCHE ZUSTANDSTEUERUNG (Aufgerufen durch die HalSensors-Fassade)
    // ========================================================================

    void setAdcIdelState();
    void setAdcWaitigForWpState();
    void setAdcCalibrationsState();

    /**
     * @brief Injektiert geladene Schwellenwerte (z.B. beim Booten aus der Datei).
     */
    void updateCalibrationThresholds(uint16_t calib_mode, uint16_t calib_value);
};

#endif // PROFILE_ANALYZER_HPP