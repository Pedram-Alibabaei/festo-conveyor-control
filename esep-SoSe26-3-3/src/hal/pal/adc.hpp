

#ifndef HAL_PAL_ADC_HPP_
#define HAL_PAL_ADC_HPP_

#include <hal/pal/device.hpp>
#include <shared/types.hpp>

namespace hal
{
namespace pal
{

class Adc
{
private:
    struct Register
    {
        enum class Offset
        {
            revision = 0x0,
            sysconfig = 0x10,
            irqstatus_raw = 0x24,
            irqstatus = 0x28,
            irqenable_set = 0x2C,
            irqenable_clr = 0x30,
            irqwakeup = 0x34,
            dmaenable_set = 0x38,
            dmaenable_clr = 0x3C,
            ctrl = 0x40,
            adcstat = 0x44,
            adcrange = 0x48,
            adc_clkdiv = 0x4C,
            adc_misc = 0x50,
            stepenable = 0x54,
            idleconfig = 0x58,
            ts_charge_stepconfig = 0x5C,
            ts_charge_delay = 0x60,
            stepconfig1 = 0x64,
            stepdelay1 = 0x68,
            stepconfig2 = 0x6C,
            stepdelay2 = 0x70,
            stepconfig3 = 0x74,
            stepdelay3 = 0x78,
            stepconfig4 = 0x7C,
            stepdelay4 = 0x80,
            stepconfig5 = 0x84,
            stepdelay5 = 0x88,
            stepconfig6 = 0x8C,
            stepdelay6 = 0x90,
            stepconfig7 = 0x94,
            stepdelay7 = 0x98,
            stepconfig8 = 0x9C,
            stepdelay8 = 0xA0,
            stepconfig9 = 0xA4,
            stepdelay9 = 0xA8,
            stepconfig10 = 0xAC,
            stepdelay10 = 0xB0,
            stepconfig11 = 0xB4,
            stepdelay11 = 0xB8,
            stepconfig12 = 0xBC,
            stepdelay12 = 0xC0,
            stepconfig13 = 0xC4,
            stepdelay13 = 0xC8,
            stepconfig14 = 0xCC,
            stepdelay14 = 0xD0,
            stepconfig15 = 0xD4,
            stepdelay15 = 0xD8,
            stepconfig16 = 0xDC,
            stepdelay16 = 0xE0,
            fifo0count = 0xE4,
            fifo0threshold = 0xE8,
            dma0req = 0xEC,
            fifo1count = 0xF0,
            fifo1threshold = 0xF4,
            dma1req = 0xF8,
            fifo0data = 0x100,
            fifo1data = 0x200,
        };

        // irqenable_set, irqenable_clr, irqstatus
        union Irq
        {
            volatile UInt32 data;
            struct
            {
                volatile UInt32 hw_pen_event_asynchronous :1;
                volatile UInt32 end_of_sequence :1;
                volatile UInt32 fifo0_threshold :1;
                volatile UInt32 fifo0_overrun :1;
                volatile UInt32 fifo0_underflow :1;
                volatile UInt32 fifo1_threshold :1;
                volatile UInt32 fifo1_overrun :1;
                volatile UInt32 fifo1_underflow :1;
                volatile UInt32 out_of_range :1;
                volatile UInt32 pen_up_event :1;
                volatile UInt32 hw_pen_event_synchronous :1;
                volatile UInt32 reserved :21;
            } field;

            /******************************************************************
             * @brief Alle Felder mit 0 initialisieren
             */
            constexpr
            Irq() noexcept :data(0)
            {
            }
        };

        union Ctrl
        {
            volatile UInt32 data;
            struct
            {
                volatile UInt32 enable :1;
                volatile UInt32 step_id_tag :1;
                volatile UInt32 stepconfig_writeprotect_n_active_low :1;
                volatile UInt32 adc_bias_select :1;
                volatile UInt32 power_down :1;
                volatile UInt32 afe_pen_ctrl :2;
                volatile UInt32 touch_screen_enable :1;
                volatile UInt32 hw_event_mapping :1;
                volatile UInt32 hw_preempt :1;
                volatile UInt32 reserved :22;
            }field;

            /******************************************************************
             * @brief Alle Felder mit 0 initialisieren
             */
            constexpr
            Ctrl() noexcept : data(0)
            {
            }
        };

        union AdcClkdiv
        {
            volatile UInt32 data;
            struct
            {
                volatile UInt32 adc_clkdiv :16;
                volatile UInt32 reserved :16;
            }field;

            /******************************************************************
             * @brief Alle Felder mit 0 initialisieren
             */
            constexpr
            AdcClkdiv() noexcept : data(0)
            {
            }
        };

        union Stepenable
        {
            volatile UInt32 data;
            struct
            {
                volatile UInt32 ts_charge :1;
                volatile UInt32 step1 :1;
                volatile UInt32 step2 :1;
                volatile UInt32 step3 :1;
                volatile UInt32 step4 :1;
                volatile UInt32 step5 :1;
                volatile UInt32 step6 :1;
                volatile UInt32 step7 :1;
                volatile UInt32 step8 :1;
                volatile UInt32 step9 :1;
                volatile UInt32 step10 :1;
                volatile UInt32 step11 :1;
                volatile UInt32 step12 :1;
                volatile UInt32 step13 :1;
                volatile UInt32 step14 :1;
                volatile UInt32 step15 :1;
                volatile UInt32 step16 :1;
                volatile UInt32 reserved :15;
            }field;

            /******************************************************************
             * @brief Alle Felder mit 0 initialisieren
             */
            constexpr
            Stepenable() noexcept : data(0)
            {
            }
        };

        // Stepconfig1 ... Stepconfig16
        union StepconfigX
        {
            volatile UInt32 data;
            struct
            {
                volatile UInt32 mode :2;
                volatile UInt32 averaging :3;
                volatile UInt32 xppsw_swc :1;
                volatile UInt32 xnnsw_swc :1;
                volatile UInt32 yppsw_swc :1;
                volatile UInt32 ynnsw_swc :1;
                volatile UInt32 xnpsw_swc :1;
                volatile UInt32 ypnsw_swc :1;
                volatile UInt32 sel_rfp_swc_2_0 :3;
                volatile UInt32 sel_inm_swc_3_0 :3;
                volatile UInt32 sel_inp_swc_3_0 :3;
                volatile UInt32 sel_rfm_swc_1_0 :3;
                volatile UInt32 diff_cntrl :1;
                volatile UInt32 fifo_select :1;
                volatile UInt32 range_check :1;
                volatile UInt32 reserved :4;
            }field;

            /******************************************************************
             * @brief Alle Felder mit 0 initialisieren
             */
            constexpr
            StepconfigX() noexcept : data(0)
            {
            }
        };

        // Stepdelay1 ... Stepdelay16
        union StepdelayX
        {
            volatile UInt32 data;
            struct
            {
                volatile UInt32 opendelay :18;
                volatile UInt32 reserved :6;
                volatile UInt32 sampledelay :8;
            }field;

            /******************************************************************
             * @brief Alle Felder mit 0 initialisieren
             */
            constexpr
            StepdelayX() noexcept : data(0)
            {
            }
        };

        // fifo0treshold, fifo1treshold
        union FifoXthreshold
        {
            volatile UInt32 data;
            struct
            {
                volatile UInt32 fifox_threshold_level :6;
                volatile UInt32 reserved :26;
            }field;

            /******************************************************************
             * @brief Alle Felder mit 0 initialisieren
             */
            constexpr
            FifoXthreshold() noexcept : data(0)
            {
            }
        };

        // fifo0data, fifo1data
        union FifoXdata
        {
            volatile UInt32 data;
            struct
            {
                volatile UInt32 adcdata :12;
                volatile UInt32 reserved0 :4;
                volatile UInt32 adcchnlid :4;
                volatile UInt32 reserved1 :12;
            }field;

            /******************************************************************
             * @brief Alle Felder mit 0 initialisieren
             */
            constexpr
            FifoXdata() noexcept : data(0)
            {
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
    Adc() = delete;

    /**************************************************************************
     * @brief Interrupt aktivieren
     */
    static void
    enable_interrupt() noexcept;

    /**************************************************************************
     * @brief Analogen Wert aus FIFO0 lesen
     */
    static UInt32
    analog_value() noexcept;

    /**************************************************************************
     * @brief Interrupt Event zurücksetzen
     */
    static void
    clear_interrupt_event() noexcept;

private:
    /**************************************************************************
     * @brief Daten aus einem Register lesen
     */
    static UInt32
    read_register(Register::Offset offset) noexcept;

    /**************************************************************************
     * @brief Daten in ein Register schreiben
     */
    static void
    write_register(Register::Offset offset, UInt32 data) noexcept;

    /**************************************************************************
     * @brief Hält statisches Mapping für den ADC Controller
     *
     * @return Zeiger auf die dem ADC Controller zugewiesene Adresse
     */
    static const std::shared_ptr<Device> &
    device() noexcept;
};;

} // namespace pal
} // namespace hal

#endif // HAL_PAL_ADC_HPP_
