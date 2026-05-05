

#ifndef HAL_PAL_GPIO_HPP_
#define HAL_PAL_GPIO_HPP_

#include <hal/pal/device.hpp>
#include <shared/types.hpp>

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

extern "C"
{
#include <hw/inout.h>
}

namespace hal
{
namespace pal
{

template <MappedAddress address, UInt32 pin>
class Gpio
{
private:
    struct Register
    {
        enum class Offset
        {
            revision = 0x0,
            sysconfig = 0x10,
            eoi = 0x20,
            irqstatus_raw_0 = 0x24,
            irqstatus_raw_1 = 0x28,
            irqstatus_0 = 0x2C,
            irqstatus_1 = 0x30,
            irqstatus_set_0 = 0x34,
            irqstatus_set_1 = 0x38,
            irqstatus_clr_0 = 0x3C,
            irqstatus_clr_1 = 0x40,
            irqwaken_0 = 0x44,
            irqwaken_1 = 0x48,
            sysstatus = 0x114,
            ctrl = 0x130,
            oe = 0x134,
            datain = 0x138,
            dataout = 0x13C,
            leveldetect_0 = 0x140,
            leveldetect_1 = 0x144,
            risingdetect = 0x148,
            fallingdetect = 0x14C,
            debounce_enable = 0x150,
            debounce_time = 0x154,
            clear_dataout = 0x190,
            set_dataout = 0x194,
        };

        union Debouncingtime
        {
            volatile UInt32 data;
            struct
            {
                volatile UInt32 debouncetime :8;
                volatile UInt32 reserved :24;
            } field;

            /******************************************************************
             * @brief Alle Felder mit 0 initialisieren
             */
            constexpr
            Debouncingtime() noexcept :data(0)
            {
            }

            /******************************************************************
             * @brief Konstruktor für Zeitumwandlung
             *
             * @param time_ms Zeit in Millisekunden
             */
            Debouncingtime(UInt32 time_ms) noexcept : data(0)
            {
                // Aus Datenblatt:
                // "Input Debouncing Value in 31 microsecond steps.
                // Debouncing Value = (DEBOUNCETIME + 1) * 31 microseconds"
                const UInt32 time_us = time_ms * 1000;
                const UInt32 debounce_value = (time_us / 31) - 1;

                field.debouncetime = debounce_value > 0xFF
                ? 0xFF
                : debounce_value;
            }
        };

        /**********************************************************************
         * @brief Additionsoperator zur Adressierung eines Registers
         *
         * @param left Peripherieadresse
         * @param right Register-Offset
         */
        friend constexpr UIntPtr
        operator+(UIntPtr left, Offset right) noexcept
        {
            return left + static_cast<UIntPtr>(right);
        }
    };

public:
    /**************************************************************************
     * @brief Konstruktor löschen, keine Instanziierung von Objekten
     */
    Gpio() = delete;

    /**************************************************************************
     * @brief Konfiguriert den Pin als Eingang
     */
    static inline void
    config_as_input() noexcept
    {
        const auto base_address = device()->io_address();
        out32(base_address + Register::Offset::oe,
                in32(base_address + Register::Offset::oe) | ~pin);
    }

    /**************************************************************************
     * @brief Konfiguriert den Pin als Ausgang
     */
    static inline void
    config_as_output() noexcept
    {
        const auto base_address = device()->io_address();
        out32(base_address + Register::Offset::oe,
                in32(base_address + Register::Offset::oe) & ~pin);
    }

    /**************************************************************************
     * @brief Prüft, ob ein Signal anliegt
     *
     * @return Wahr bei Signal
     */
    static inline bool
    is_data_set() noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        return in32(base_address + Register::Offset::datain) & pin;
    }

    /**************************************************************************
     * @brief Schaltet den Pin ein
     */
    static inline void
    data_set() noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        out32(base_address + Register::Offset::set_dataout, pin);
    }

    /**************************************************************************
     * @brief Schaltet den Pin aus
     */
    static inline void
    data_clear() noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        out32(base_address + Register::Offset::clear_dataout, pin);
    }

    /**************************************************************************
     * @brief Konfiguriert eine Interrupt-Erkennung bei steigender Flanke
     */
    static void
    config_interrupt_rising() noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        out32(base_address + Register::Offset::risingdetect,
                in32(base_address + Register::Offset::risingdetect) | pin);
    }

    /**************************************************************************
     * @brief Konfiguriert eine Interrupt-Erkennung bei fallender Flanke
     */
    static void
    config_interrupt_falling() noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        out32(base_address + Register::Offset::fallingdetect,
                in32(base_address + Register::Offset::fallingdetect) | pin);
    }

    /**************************************************************************
     * @brief Löscht das Interrupt-Flag für diesen Pin
     */
    static void
    clear_interrupt_status() noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        out32(base_address + Register::Offset::irqstatus_1, pin);
    }

    /**************************************************************************
     * @brief Aktiviert den Interrupt für diesen Pin
     */
    static void
    enable_interrupt() noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        out32(base_address + Register::Offset::irqstatus_set_1, pin);
    }

    /**************************************************************************
     * @brief Deaktiviert den Interrupt für diesen Pin
     */
    static void
    disable_interrupt() noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        out32(base_address + Register::Offset::irqstatus_1, pin);
    }

    /**************************************************************************
     * @brief Liest den aktuellen rohen Interrupt-Status
     *
     * @return Wert der IRQSTATUS_1-Registerbits
     */
    static UInt32
    read_interrupt_status() noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        return in32(base_address + Register::Offset::irqstatus_1);
    }

    /**************************************************************************
     * @brief Aktiviert Debounce für diesen Pin
     */
    static void
    enable_debounce() noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        out32(base_address + Register::Offset::debounce_enable,
                in32(base_address + Register::Offset::debounce_enable) | pin);
    }

    /**************************************************************************
     * @brief Deaktiviert Debounce für diesen Pin
     */
    static void
    disable_debounce() noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        out32(base_address + Register::Offset::debounce_enable,
                in32(base_address + Register::Offset::debounce_enable) & ~pin);
    }

    /**************************************************************************
     * @brief Zeit für das Entprellen setzen
     *
     * @param time_ms Zeit in Millisekunden
     */
    static void
    debounce_time_ms(UInt32 time_ms) noexcept
    {
        assert(Device::is_mapped(address));
        const auto base_address = device()->io_address();
        const typename Register::Debouncingtime debouncingtime(time_ms);
        out32(base_address + Register::Offset::debounce_time,
                debouncingtime.data);
    }

private:
    /**************************************************************************
     * @brief Hält statisches Mapping für die template-Instanz der GPIO Bank
     *
     * @return Zeiger auf die der GPIO Bank zugewiesene Adresse
     */
    static inline const std::shared_ptr<Device> &
    device() noexcept
    {
        static_assert(
                address == MappedAddress::gpio_0 ||
                address == MappedAddress::gpio_1 ||
                address == MappedAddress::gpio_2,
                "Die Adresse muss einem GPIO zugeordnet sein!"
        );

        static const auto base_address = Device::map_memory(address, 0x1000);

        return base_address;
    }
};;

} // namespace pal
} // namespace hal

#endif // HAL_PAL_GPIO_HPP_
