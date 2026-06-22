
#include "adc.hpp"

#include <hal/pal/device.hpp>
#include <shared/types.hpp>

extern "C"
{
#include <hw/inout.h>
}

using namespace hal::pal;

void
Adc::enable_interrupt() noexcept
{
    // adc_clkdiv
    /*
     * module_clk = 24 MHz
     * input_clk = 3 Mhz
     * adc_clkdiv = module_clk / input_clk - 1 = 7
     */
    Register::AdcClkdiv adc_clkdiv;
    adc_clkdiv.field.adc_clkdiv = 7;
    write_register(Register::Offset::adc_clkdiv, adc_clkdiv.data);

    // ctrl
    Register::Ctrl ctrl;
    ctrl.field.enable = 1;
    ctrl.field.step_id_tag = 1;
    ctrl.field.stepconfig_writeprotect_n_active_low = 1;
    write_register(Register::Offset::ctrl, ctrl.data);

    // stepconfig1
    enum Averaging
    {
        average_no_samples = 0,
        average_2_samples,
        average_4_samples,
        average_8_samples,
        average_16_samples,
    };
    Register::StepconfigX stepconfig1;
    stepconfig1.field.mode = 1;            // SW enabled, one-shot
    stepconfig1.field.averaging = average_16_samples;
    stepconfig1.field.sel_rfp_swc_2_0 = 1; // VDDA
    stepconfig1.field.xppsw_swc = 1;       // pull up AN0
    stepconfig1.field.xnnsw_swc = 1;       // pull down AN1
    write_register(Register::Offset::stepconfig1, stepconfig1.data);

    // stepdelay1
    Register::StepdelayX stepdelay1;
    stepdelay1.field.opendelay = 0xFF;
    stepdelay1.field.sampledelay = 0x0;
    write_register(Register::Offset::stepdelay1, stepdelay1.data);

    // stepenable
    Register::Stepenable stepenable;
    stepenable.field.step1 = 1;
    write_register(Register::Offset::stepenable, stepenable.data);

    // fifo0threshold
    Register::FifoXthreshold fifo0threshold;
    fifo0threshold.field.fifox_threshold_level = 1;
    write_register(Register::Offset::fifo0threshold, fifo0threshold.data);

    // irq
    Register::Irq irq;
    irq.field.end_of_sequence = 1;
    write_register(Register::Offset::irqstatus, irq.data);     // clear
    write_register(Register::Offset::irqenable_set, irq.data); // enable
}

void
Adc::trigger_next_sample() noexcept
{
    Register::Stepenable stepenable;
    stepenable.field.step1 = 1;
    write_register(Register::Offset::stepenable, stepenable.data);
}

UInt32
Adc::analog_value() noexcept
{
    Register::FifoXdata fifo { };
    fifo.data = read_register(Register::Offset::fifo0data);
    return fifo.field.adcdata;
}

void
Adc::clear_interrupt_event() noexcept
{
    Register::Irq irq { };
    irq.field.end_of_sequence = 1;
    write_register(Register::Offset::irqstatus, irq.data);
}

UInt32
Adc::read_register(Register::Offset offset) noexcept
{
    const auto base_address = device()->io_address();
    return in32(base_address + offset);
}

void
Adc::write_register(Register::Offset offset, UInt32 data) noexcept
{
    const auto base_address = device()->io_address();
    out32(base_address + offset, data);
}

const std::shared_ptr<Device> &
Adc::device() noexcept
{
    static const auto base_address = Device::map_memory(MappedAddress::adc_tsc,
                                                        0x2000);

    return base_address;
}
