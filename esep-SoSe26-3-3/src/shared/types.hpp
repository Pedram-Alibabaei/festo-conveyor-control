/**
 * @file types.hpp
 * @brief Globale Typdefinitionen, Enums und Datenstrukturen für die Förderanlage.
 *
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef TYPES_HPP_
#define TYPES_HPP_

#include <cstddef>
#include <cstdint>
#include <sys/iomsg.h>

// ====================================================================
// 1. STANDARD-DATENTYPEN (Aliases für feste Bitbreiten)
// ====================================================================

using SInt8  = std::int8_t;
using SInt16 = std::int16_t;
using SInt32 = std::int32_t;
using SInt64 = std::int64_t;

using UInt8  = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using UInt64 = std::uint64_t;

using Size    = std::size_t;
using UIntPtr = std::uintptr_t;

// ====================================================================
// 2. WERKSTÜCK-EIGENSCHAFTEN & ENUMS
// ====================================================================

/**
 * @brief Alle bekannten Werkstücktypen der Anlage.
 */
enum class WpType : std::uint16_t {
    UNKNOWN             = _PULSE_CODE_MINAVAIL + 50, ///< Startzustand, noch nicht vermessen
    FLAT                = _PULSE_CODE_MINAVAIL + 51, ///< Flaches Werkstück
    TALL                = _PULSE_CODE_MINAVAIL + 52, ///< Hohes Werkstück ohne Bohrung
    TALL_DRILLED        = _PULSE_CODE_MINAVAIL + 53, ///< Hohes Werkstück mit Bohrung
    TALL_DRILLED_METAL  = _PULSE_CODE_MINAVAIL + 54, ///< Hohes Werkstück mit Bohrung und Metall
    TALL_METAL          = _PULSE_CODE_MINAVAIL + 55  ///< Hohes Werkstück aus reinem Metall
};

/**
 * @brief Alle physischen Zonen und Abschnitte auf dem Förderband.
 */
enum class WpLocation : std::uint16_t {
    AT_ENTRY           = _PULSE_CODE_MINAVAIL + 60, ///< Am Einlauf (Lichtschranke blockiert)
    TRANSIT_TO_HEIGHT  = _PULSE_CODE_MINAVAIL + 61, ///< Fahrt vom Einlauf zum Höhensensor
    AT_HEIGHT_SENSOR   = _PULSE_CODE_MINAVAIL + 62, ///< Unter dem Höhensensor (Messung aktiv)
    TRANSIT_TO_EJECTOR = _PULSE_CODE_MINAVAIL + 63, ///< Fahrt vom Höhensensor zur Sortierweiche
    AT_EJECTOR         = _PULSE_CODE_MINAVAIL + 64, ///< An der Weiche (Sortierentscheidung)
    TRANSIT_TO_CHUTE   = _PULSE_CODE_MINAVAIL + 65, ///< Werkstück rutscht gerade die Rutsche runter
    AT_CHUTE           = _PULSE_CODE_MINAVAIL + 66, ///< Werkstück liegt physisch am Ende der Rutsche
    TRANSIT_TO_END     = _PULSE_CODE_MINAVAIL + 67, ///< Fahrt von der Weiche zum Bandende
    AT_END             = _PULSE_CODE_MINAVAIL + 68, ///< Am Auslauf (Bereit zur Übergabe an Anlage 2)
    UNKNOWN_LOCATION   = _PULSE_CODE_MINAVAIL + 69
};

/**
 * @brief Definition der physischen Positionen im Gesamtsystem.
 */
enum class AnlagenPosition : std::uint16_t {
    VORN_ANLAGE_1      = _PULSE_CODE_MINAVAIL + 1,  ///< Erste Teilanlage (bzw. Master)
    HINTEN_ANLAGE_2    = _PULSE_CODE_MINAVAIL + 2,  ///< Zweite Teilanlage (bzw. Slave)
    UNKNOWN_POSITION   = _PULSE_CODE_MINAVAIL + 3   ///< Undefinierte oder fehlerhafte Position
};

// ====================================================================
// 3. KALIBRIERUNGSDATEN (Zentrales Datentransfer-Objekt)
// ====================================================================

/**
 * @brief Struktur für die kalibrierten Zeit-Takte aus dem Service-Modus.
 * @note Enthält ausschließlich reine 10ms-Timer-Takte. Wird vom ServiceState befüllt,
 * vom CalibrationManager persistent gespeichert und vom WorkpieceManager gelesen.
 */
struct CalibrationData {
    // Direkt eingelesene Werte (Messung während des Service-Modus)
// 1. HÖHENMESSUNG (ADC-Referenzen)
    std::uint16_t adc_emptyBelt = 0;
    std::uint16_t adc_flatCenter = 0;
    std::uint32_t limit_minSamplesForClass = 0;

    std::uint32_t ticks_EntryC_to_HeightB_Slow = 0;
    std::uint32_t ticks_HeightB_to_HeightC_Slow = 0;
    std::uint32_t ticks_HeightC_to_EjectorB_Slow = 0;
    std::uint32_t ticks_EjectorB_to_EjectorC_Slow = 0;
    std::uint32_t ticks_EjectorC_to_ChuteB_Slow = 0;

    std::uint32_t ticks_EntryC_to_HeightB_Normal = 0;
    std::uint32_t ticks_HeightB_to_HeightC_Normal = 0;
    std::uint32_t ticks_HeightC_to_EjectorB_Normal = 0;
    std::uint32_t ticks_EjectorB_to_EjectorC_Normal = 0;
    std::uint32_t ticks_EjectorC_to_EndB_Normal = 0;
    std::uint32_t ticks_EndB_to_EndC_Normal = 0;
};

#endif // TYPES_HPP_