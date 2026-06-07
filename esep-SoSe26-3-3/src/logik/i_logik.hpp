/**
 * @file i_Logik.hpp
 * @brief Interface für die zentrale Steuerungs-Zustandsmaschine (Logik) der Festo-Anlage.
 *
 * @details Definiert die Kommunikationsschnittstelle der Logikkomponente (Schicht 1 FSM).
 * Sie nimmt Ereignisse der HAL, Netzwerkdaten des Dispatchers sowie das fertige
 * Kalibrierungspaket entgegen und steuert den digitalen Zwilling.
 *
 * Embedded Systems Engineering WiSe2025 Team 1.1
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
     * @note Steuert die dynamische Rollenverteilung (Anlage 1 vs. Anlage 2) zur Laufzeit.
     * @param pos Die Position im Gesamtsystem (VORN_ANLAGE_1 oder HINTEN_ANLAGE_2).
     */
    virtual void init(AnlagenPosition pos) = 0;

    /**
     * @brief Nimmt ein Ereignis von der HAL oder dem Framework entgegen und triggert die Haupt-FSM.
     * @param e Das aufgetretene Ereignis (z.B. logik::Event::HEIGHT_BLOCKED).
     */
    virtual void processEvent(logik::Event e) = 0;

    /**
     * @brief Übergibt den von der HAL (Analog-Auswertung) ermittelten Typ eines Werkstücks.
     * @note Wird exklusiv bei Beendigung des Messvorgangs (HEIGHT_CLEARED) an die Logik gereicht.
     * @param type Der erkannte physische Werkstücktyp.
     */
    virtual void workpieceTypeFromHS(WpType type) = 0;

    /**
     * @brief Verarbeitet Werkstück-Informationen (ID und Typ), die via QNX-Netzwerk empfangen wurden.
     * @note Wird auf Anlage 2 aufgerufen, um den digitalen Zwilling am Einlauf synchron zu gebären (Tracing).
     * @param id Die von Anlage 1 vergebene, eindeutige Werkstück-ID.
     * @param type Der bereits auf Anlage 1 vor-analysierte Werkstücktyp.
     */
    virtual void workpieceInfo(int id, WpType type) = 0;

    /**
     * @brief Aktualisiert das Rezept der internen Werkstück-Liste nach einer Manipulation.
     * @note Triggert auf Anlage 1 die Methode forceType(), um das Rezept absolut auf den Soll-Typen zu zwingen.
     * @param type Der Typ des Werkstücks, auf den die Rezeptur synchronisiert werden muss.
     */
    virtual void updateWpListe(WpType type) = 0;

    /**
     * @brief Der zentrale 10ms-Herzschlag des Bandes für interne Wegstrecken-Timer und Watchdogs.
     * @note Reicht den Hardware-Timer-Interrupt direkt an das Schieberegister (takt) weiter.
     */
    virtual void takt() = 0;

    /**
     * @brief Aktualisiert den Bereitschafts- bzw. Füllstandstatus der Rutsche der Partneranlage.
     * @param isFull true, wenn die Rutsche blockiert/voll ist, andernfalls false.
     */
    virtual void setPartnerChuteStatus(bool isFull) = 0;

    // ========================================================================
    // NEUE ZENTRALE KALIBRIERUNGS-SCHNITTSTELLE
    // ========================================================================

    /**
     * @brief Übergibt die im Service-Modus live gemessenen ADC-Kalibrierungswerte.
     * @note Verhindert das Überschreiben der bestehenden Wegstrecken-Ticks im Schieberegister.
     * @param emptyBelt Der gemessene ADC-Wert für das leere Band.
     * @param flatCenter Der gemessene ADC-Wert für ein flaches Werkstück.
     * @param minSamples Die gemessene Mindestanzahl an Samples (Länge des Teils).
     */
    virtual void setLiveAdcCalibration(uint16_t emptyBelt, uint16_t flatCenter, uint32_t minSamples) = 0;
};

#endif // I_LOGIK_HPP