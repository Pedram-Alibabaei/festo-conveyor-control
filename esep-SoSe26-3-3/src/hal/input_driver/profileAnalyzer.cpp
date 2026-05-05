/**
 * @file ProfileAnalyzer.cpp
 *
 * @brief Implementierung der ProfileAnalyzer-Klasse.
 *
 * Diese Datei enthält die Logik zur Rauschunterdrückung (Moving Average Filter)
 * und zur profilbasierten, geschwindigkeitsunabhängigen Werkstückerkennung.
 */

#include "ProfileAnalyzer.hpp"
#include <shared/types.hpp>
// ============================================================================
// Konstruktor & Setup
// ============================================================================

/**
 * @brief Standardkonstruktor.
 * Setzt das gesamte Gedächtnis, den Filter und die Kalibrierung auf sichere Startwerte (0).
 */
ProfileAnalyzer::ProfileAnalyzer() {
    // 1. Filter zurücksetzen
    for (int i = 0; i < 5; i++) {
        m_filter_buffer[i] = 0;
    }
    m_filter_index   = 0;
    m_filter_is_full = false;

    // 2. Gedächtnis zurücksetzen
    m_current_peak_height = 0;
    m_hole_detected       = false;
    m_empty_belt_counter  = 0;
    m_is_measuring        = false;

    // 3. Kalibrierung nullen
    m_trigger_threshold = 0;
    m_flat_min = 0; m_flat_max = 0;
    m_tall_min = 0; m_tall_max = 0;
    m_hole_min = 0; m_hole_max = 0;
    m_empty_count_limit = 15;
}

/**
 * @brief Übernimmt die System-Kalibrierung aus der Konfigurationsdatei.
 */
void ProfileAnalyzer::setCalibration(uint32_t trigger,
                                     uint32_t flatMin, uint32_t flatMax,
                                     uint32_t tallMin, uint32_t tallMax,
                                     uint32_t holeMin, uint32_t holeMax,
                                     uint32_t emptyCountLimit) {
    m_trigger_threshold = trigger;
    m_flat_min = flatMin;
    m_flat_max = flatMax;
    m_tall_min = tallMin;
    m_tall_max = tallMax;
    m_hole_min = holeMin;
    m_hole_max = holeMax;
    m_empty_count_limit = emptyCountLimit;
}

/**
 * @brief Setzt die Klasse in den Bereitschaftsmodus für das nächste Werkstück.
 * Leert den Filter-Puffer und löscht das Profilgedächtnis der letzten Messung.
 */
void ProfileAnalyzer::reset() {
    // Filter leeren
    for (int i = 0; i < 5; i++) {
        m_filter_buffer[i] = 0;
    }
    m_filter_index   = 0;
    m_filter_is_full = false;

    // Gedächtnis leeren
    m_current_peak_height = 0;
    m_hole_detected       = false;
    m_empty_belt_counter  = 0;
    m_is_measuring        = false;
}

// ============================================================================
// Die Hauptlogik (Wird für jeden rohen ADC-Wert 1x aufgerufen)
// ============================================================================

/**
 * @brief Verarbeitet einen neuen ADC-Messwert.
 * Wendet zuerst einen gleitenden Mittelwert an und analysiert danach das Profil.
 */
void ProfileAnalyzer::processSample(uint32_t raw_adc_value) {

    // ------------------------------------------------------------------------
    // STUFE 1: SIGNAL-GLÄTTUNG (Moving Average Filter)
    // ------------------------------------------------------------------------

    // Wert in den Ringpuffer schreiben und Index weiterrücken
    m_filter_buffer[m_filter_index] = raw_adc_value;
    m_filter_index++;

    // Puffer-Überlauf behandeln (Ringstruktur)
    if (m_filter_index >= 5) {
        m_filter_index = 0;
        m_filter_is_full = true; // Ab jetzt sind immer 5 gültige Werte im Puffer
    }

    // Solange wir noch keine 5 Werte für einen sauberen Durchschnitt haben,
    // ignorieren wir die Eingabe und warten auf das nächste Sample.
    if (!m_filter_is_full) {
        return;
    }

    // Durchschnitt berechnen (Rauschen herausfiltern)
    uint32_t sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += m_filter_buffer[i];
    }
    uint32_t smoothed_value = sum / 5;


    // ------------------------------------------------------------------------
    // STUFE 2: TOPOGRAFIE-ERKENNUNG (Mit dem geglätteten Wert!)
    // ------------------------------------------------------------------------

    // Fall A: Wir warten noch auf ein Werkstück (Trigger-Erkennung)
    if (!m_is_measuring) {
        if (smoothed_value > m_trigger_threshold) {
            // Startschuss: Das Werkstück ist da!
            m_is_measuring = true;
            m_current_peak_height = smoothed_value;
            m_hole_detected = false;
            m_empty_belt_counter = 0;
        }
        return; // Für diesen Zyklus fertig
    }

    // Fall B: Wir messen gerade ein Werkstück
    if (m_is_measuring) {

        // 1. Gipfel-Tracking (Neuen maximalen Höhenrekord speichern)
        if (smoothed_value > m_current_peak_height) {
            m_current_peak_height = smoothed_value;
        }

        // 2. Bohrungs-Erkennung (Tal-Tracking)
        // Nur wenn das Werkstück grundsätzlich hoch ist, suchen wir nach einer Bohrung
        if (m_current_peak_height >= m_tall_min) {
            if (smoothed_value >= m_hole_min && smoothed_value <= m_hole_max) {
                m_hole_detected = true;
            }
        }

        // 3. Abriss-Erkennung (Fährt das Teil gerade aus dem Lichtschrankenbereich?)
        if (smoothed_value <= m_trigger_threshold) {
            m_empty_belt_counter++; // Band scheint wieder leer zu sein
        } else {
            // Wert ist wieder über den Trigger gestiegen -> Abbruch der Hysterese
            m_empty_belt_counter = 0;
        }
    }
}

// ============================================================================
// Auswertung & Getter
// ============================================================================

/**
 * @brief Prüft anhand der Hysterese, ob die Messung komplett abgeschlossen ist.
 */
bool ProfileAnalyzer::isMeasurementComplete() const {
    return m_is_measuring && (m_empty_belt_counter >= m_empty_count_limit);
}

/**
 * @brief Fällt das finale Urteil über den Werkstücktyp anhand des gesammelten Profils.
 */
WpType ProfileAnalyzer::determineFinalType() const {
    WpType final_type = WpType::UNKNOWN;

    // 1. Ist der Gipfel im Flach-Bereich?
    if (m_current_peak_height >= m_flat_min && m_current_peak_height <= m_flat_max) {
        final_type = WpType::FLAT;
    }
    // 2. Ist der Gipfel im Hoch-Bereich?
    else if (m_current_peak_height >= m_tall_min && m_current_peak_height <= m_tall_max) {
        // Wurde zusätzlich ein Tal (Bohrung) gefunden?
        if (m_hole_detected) {
            final_type = WpType::TALL_DRILLED;
        } else {
            final_type = WpType::TALL;
        }
    }

    return final_type;
}
