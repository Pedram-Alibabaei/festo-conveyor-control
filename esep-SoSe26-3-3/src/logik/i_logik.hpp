/**
 * @file i_Logik.hpp
 * @brief Interface für die zentrale Steuerungs-Zustandsmaschine (Logik) der Festo-Anlage.
 */

#ifndef I_LOGIK_HPP
#define I_LOGIK_HPP

#include "shared/logikEvent.hpp"
#include "shared/types.hpp"
#include <cstdint>

class i_Logik {
public:
    virtual ~i_Logik() = default;

    /**
     * @brief Initialisiert die Logik-Komponente mit der physischen Position der Anlage.
     * @param pos Die Position im System (Anlage 1 oder Anlage 2).
     */
    virtual void init(AnlagenPosition pos) = 0;

    /**
     * @brief Nimmt ein Ereignis vom Dispatcher entgegen und triggert den Zustandsautomaten.
     * @param e Das aufgetretene Ereignis (z.B. Lichtschranke unterbrochen).
     */
    virtual void processEvent(logik::Event e) = 0;

    /**
     * @brief Übergibt den vom Höhenmessungs-Sensor (HS) ermittelten Typ des Werkstücks.
     * @param type Der erkannte Typ (z.B. flach, hoch, mit Bohrung).
     */
    virtual void workpieceTypeFromHS(WpType type) = 0;

    /**
     * @brief Verarbeitet Werkstück-Informationen, die über das Netzwerk empfangen wurden.
     * @param id Die eindeutige ID des Werkstücks.
     * @param type Der Typ des Werkstücks.
     */
    virtual void workpieceInfo(int id, WpType type) = 0;

    /**
     * @brief Aktualisiert die interne Werkstück-Liste (z.B. nach einer Übergabe oder Aussortierung).
     * @param type Der Typ des betroffenen Werkstücks.
     */
    virtual void updateWpListe(WpType type) = 0;

    /**
     * @brief Takt-Signal des Bandes für interne Wegstrecken-Timer und Überwachung.
     */
    virtual void tackt() = 0;

	virtual void setPartnerChuteStatus(bool isFull) = 0;

    // ========================================================================
    // KALIBRIERUNGS-SCHNITTSTELLEN
    // ========================================================================

    /**
     * @brief Übergibt die beim Booten geladenen Wegstrecken-Ticks an die Logik.
     * @param calibMode Das exakte Segment aus ipc::CalibMode (z.B. TICK_ENTRY_C_TO_HEIGHT_B).
     * @param tickValue Die geladene Anzahl an Band-Ticks für diese Strecke.
     */
    virtual void setTrackedSegmentTicks(uint16_t calibMode, uint16_t tickValue) = 0;

    /**
     * @brief Empfängt die rohen ADC-Kalibrierungsdaten von der HAL während des Live-Einlernens.
     * @param calibMode Der aktuelle Kalibrierungsmodus (z.B. ipc::CalibMode::EMPTY_BELT).
     * @param adcValue Der gemessene Rohwert vom ADC-Sensor.
     */
    virtual void receiveRawCalibrationData(uint16_t calibMode, uint16_t adcValue) = 0;
};

#endif // I_LOGIK_HPP