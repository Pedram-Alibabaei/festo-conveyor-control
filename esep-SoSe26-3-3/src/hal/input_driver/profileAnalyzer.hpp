/**
 * @file ProfileAnalyzer.hpp
 *
 * @brief Klassenbeschreibung: ProfileAnalyzer
 *
 * Die Klasse ProfileAnalyzer ist ein reines Logik-Modul ("Number Cruncher") zur
 * Auswertung analoger Höhenprofile. Sie ist vollständig entkoppelt von der Hardware
 * (HAL) und dem Betriebssystem (QNX), was sie hochgradig testbar macht.
 *
 * Hauptaufgaben (Zuständigkeiten):
 * - Signalglättung (Smoothing): Nutzt einen gleitenden Mittelwert (Moving Average
 *   Filter) über die letzten 5 Messwerte, um elektromagnetisches Rauschen oder
 *   mechanisches Wackeln (Spikes) in Echtzeit herauszufiltern.
 * - Topografie-Analyse: Erkennt Werkstücke geschwindigkeitsunabhängig, indem
 *   ausschließlich Extremwerte (höchster Gipfel, tiefstes Tal) während des Mess-
 *   fensters analysiert werden, anstatt statische Sample-Mengen zu zählen.
 * - Hysterese-Überwachung: Bestimmt über einen Zähler den sicheren Abriss
 *   (Ende der Messung), sobald das Förderband wieder zuverlässig leer ist.
 */

#ifndef PROFILE_ANALYZER_HPP
#define PROFILE_ANALYZER_HPP

#include <cstdint>
// Hier bindest du eure Datei ein, in der das WpType-Enum (UNKNOWN, FLAT, TALL, DRILLED) definiert ist
#include <shared/types.hpp>

class ProfileAnalyzer {
private:
    // ========================================================================
    // 1. Signal-Filter (Gleitender Mittelwert)
    // ========================================================================
    uint32_t m_filter_buffer[5];  // Ringpuffer für die letzten 5 Werte
    uint8_t  m_filter_index;      // Aktuelle Schreibposition im Puffer
    bool     m_filter_is_full;    // True, sobald die ersten 5 Werte gesammelt wurden

    // ========================================================================
    // 2. Das "Gedächtnis" für das aktuelle Werkstück
    // ========================================================================
    uint32_t m_current_peak_height; // Der absolut höchste gemessene, geglättete Wert
    bool     m_hole_detected;       // Wurde zwischendurch ein signifikantes Tal erkannt?
    uint32_t m_empty_belt_counter;  // Zählt, wie lange das Band am Stück leer ist (Hysterese)
    bool     m_is_measuring;        // Status-Flag: Befindet sich aktuell ein Teil unter dem Sensor?

    // ========================================================================
    // 3. Kalibrierungsdaten (Werden beim Systemstart aus einer Datei geladen)
    // ========================================================================
    uint32_t m_trigger_threshold;   // Ab welchem ADC-Wert beginnt ein Werkstück?

    // Toleranzbänder (Min/Max) für die verschiedenen Zonen
    uint32_t m_flat_min;
    uint32_t m_flat_max;

    uint32_t m_tall_min;
    uint32_t m_tall_max;

    uint32_t m_hole_min;
    uint32_t m_hole_max;

    uint32_t m_empty_count_limit;   // Wie viele "Leer"-Messungen bedeuten das Ende der Messung?

public:
    /**
     * @brief Konstruktor: Initialisiert alle Werte sicher mit 0.
     */
    ProfileAnalyzer();

    /**
     * @brief Setzt die System-Kalibrierung (Schwellenwerte und Zonen).
     * Wird idealerweise beim Systemstart vom main-Thread aufgerufen.
     */
    void setCalibration(uint32_t trigger,
                        uint32_t flatMin, uint32_t flatMax,
                        uint32_t tallMin, uint32_t tallMax,
                        uint32_t holeMin, uint32_t holeMax,
                        uint32_t emptyCountLimit);

    /**
     * @brief Setzt das interne Gedächtnis und den Filter zurück.
     * Muss aufgerufen werden, nachdem eine Messung erfolgreich beendet wurde.
     */
    void reset();

    /**
     * @brief Nimmt einen rohen ADC-Wert entgegen, glättet ihn und aktualisiert das Höhenprofil.
     * @param raw_adc_value Der rohe, ungefilterte Wert direkt aus dem Hardware-Register.
     */
    void processSample(UInt32 raw_adc_value);

    /**
     * @brief Prüft, ob das Werkstück den Sensorbereich vollständig verlassen hat.
     * @return True, wenn die Hysterese-Bedingung (Band ist lange genug leer) erfüllt ist.
     */
    bool isMeasurementComplete() const;

    /**
     * @brief Wertet das aufgezeichnete Profil aus und fällt das finale Urteil.
     * Darf erst aufgerufen werden, wenn isMeasurementComplete() true zurückgibt.
     * @return Der ermittelte Werkstücktyp (WpType).
     */
    WpType determineFinalType() const;
};

#endif // PROFILE_ANALYZER_HPP
