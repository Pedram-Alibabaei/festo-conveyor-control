/**
 * @file I_CalibrationManager.hpp
 * @brief Flexibles Interface fuer die persistente Werte-Kalibrierung (Key-Value-Store).
 * Arbeitet pfadunabhaengig ueber den im Konstruktor definierten Standardpfad.
 */

#ifndef I_CALIBRATION_MANAGER_HPP
#define I_CALIBRATION_MANAGER_HPP

#include <string>

namespace calib {
    // ========================================================================
    // ZENTRALE SCHLÜSSEL-NAMEN (Deine reduzierte, exakte Event-Kette)
    // ========================================================================

    // 1. HÖHENMESSUNG (ADC-Werte & Fenster)
    const std::string EMPTY_BELT                 = "Adc_EmptyBelt";
    const std::string FLAT_CENTER                = "Adc_FlatCenter";
    const std::string MIN_SAMPLES_FOR_CLASS      = "Limit_MinSamplesForClass";

    // 2. ZEIT-TRACKING (Logische Band-Takte / Ticks)
    const std::string TICKS_ENTRY_C_TO_HEIGHT_B   = "Ticks_EntryCleared_to_HeightBlocked";
    const std::string TICKS_HEIGHT_B_TO_HEIGHT_C  = "Ticks_HeightBlocked_to_HeightCleared";
    const std::string TICKS_HEIGHT_C_TO_EJECTOR_B = "Ticks_HeightCleared_to_EjectorBlocked";
    const std::string TICKS_EJECTOR_B_TO_EJECTOR_C = "Ticks_EjectorBlocked_to_EjectorCleared";
    const std::string TICKS_EJECTOR_C_TO_END_B    = "Ticks_EjectorCleared_to_EndBlocked";
    const std::string TICKS_END_B_TO_END_C        = "Ticks_EndBlocked_to_EndCleared";
}

class i_CalibrationManager {
public:
    virtual ~i_CalibrationManager() = default;

    // ========================================================================
    // DATEI-OPERATIONEN (Nutzen den intern im Konstruktor gesetzten Pfad)
    // ========================================================================

    /**
     * @brief Laedt die Kalibrierungsdatei vom intern gesetzten Standardpfad.
     */
    virtual void load() = 0;

    /**
     * @brief Speichert alle aktuellen Key-Value-Paare permanent auf den Standardpfad.
     */
    virtual void save() = 0;

    /**
     * @brief Prueft, ob eine Kalibrierungsdatei auf dem Standardpfad existiert.
     * @return true, wenn die Datei existiert, ansonsten false.
     */
    virtual bool fileExists() = 0;

    /**
     * @brief Verpackt alle geladenen Werte als spezifische Pulse und schickt sie
     * zur automatischen Weiterverteilung an den Dispatcher.
     * @param dispatcherCoid Der Verbindungs-Kanal (Connection ID) zum Dispatcher.
     */
    virtual void distributeValues(int dispatcherCoid) = 0;


    // ========================================================================
    // DATEN-OPERATIONEN (RAM-Speicher / Key-Value-Store)
    // ========================================================================

    /**
     * @brief Sucht nach einem bestimmten Schluessel in den geladenen RAM-Daten.
     * @return true, wenn der Wert existiert, ansonsten false.
     */
    virtual bool hasKey(const std::string& key) = 0;

    /**
     * @brief Liest einen ganzzahligen Wert aus dem RAM aus.
     * @param key Der eindeutige Name des Wertes aus dem calib:: Namespace.
     * @param defaultValue Rueckgabewert, falls der Schluessel nicht existiert.
     */
    virtual int getInt(const std::string& key, int defaultValue = 0) = 0;

    /**
     * @brief Fuegt einen neuen Wert hinzu ODER bearbeitet einen bestehenden Wert im RAM.
     * Aenderungen sind fluechtig, bis save() aufgerufen wird.
     * @param key Der eindeutige Name des Wertes aus dem calib:: Namespace.
     * @param value Der neue zu speichernde Ganzzahl-Wert.
     */
    virtual void setInt(const std::string& key, int value) = 0;

    /**
     * @brief Loescht alle aktuellen Werte aus dem temporaeren RAM-Speicher.
     */
    virtual void clear() = 0;
};

#endif // I_CALIBRATION_MANAGER_HPP