/*
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef TYPES_HPP_
#define TYPES_HPP_

#include <cstddef>
#include <cstdint>

// signed int
using SInt8 = std::int8_t;
using SInt16 = std::int16_t;
using SInt32 = std::int32_t;
using SInt64 = std::int64_t;

// unsigned int
using UInt8 = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using UInt64 = std::uint64_t;

// für Speicherzugriff
using Size = std::size_t;
using UIntPtr = std::uintptr_t;

// Alle bekannten Werkstücktypen der Anlage
enum class WpType {
 UNKNOWN,             // Startzustand, noch nicht vermessen
 FLAT,                // Flaches Werkstück
 TALL,                // Hohes Werkstück ohne Bohrung
 TALL_DRILLED,        // Hohes Werkstück mit Bohrung
 TALL_DRILLED_METAL,  // Hohes Werkstück mit Bohrung und metallisch
 TALL_METAL           // Hohes Werkstück metallisch
};

#endif // TYPES_HPP_
