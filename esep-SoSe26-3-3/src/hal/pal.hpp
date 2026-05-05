/*
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef HAL_PAL_HPP_
#define HAL_PAL_HPP_

#include <hal/pal/adc.hpp>
#include <hal/pal/gpio.hpp>
#include <hal/pal/mapped_address.hpp>
#include <shared/types.hpp>

namespace hal
{
namespace pal
{

namespace ii // in interface
{
constexpr UInt32 pin_mask_estop = 1 << 27;
constexpr UInt32 pin_mask_reset = 1 << 26;
constexpr UInt32 pin_mask_start = 1 << 22;
constexpr UInt32 pin_mask_stop = 1 << 23;

using Estop = Gpio<MappedAddress::gpio_0, pin_mask_estop>;
using Reset = Gpio<MappedAddress::gpio_0, pin_mask_reset>;
using Start = Gpio<MappedAddress::gpio_0, pin_mask_start>;
using Stop = Gpio<MappedAddress::gpio_0, pin_mask_stop>;
} // namespace ii

namespace ip // in position
{
constexpr UInt32 pin_mask_chute = 1 << 15;
constexpr UInt32 pin_mask_ejector = 1 << 5;
constexpr UInt32 pin_mask_entry = 1 << 2;
constexpr UInt32 pin_mask_end = 1 << 20;
constexpr UInt32 pin_mask_metal = 1 << 7;

using Chute = Gpio<MappedAddress::gpio_0, pin_mask_chute>;
using Ejector = Gpio<MappedAddress::gpio_0, pin_mask_ejector>;
using Entry = Gpio<MappedAddress::gpio_0, pin_mask_entry>;
using End = Gpio<MappedAddress::gpio_0, pin_mask_end>;
using Height = Adc;
using Metal = Gpio<MappedAddress::gpio_0, pin_mask_metal>;
} // namespace ip

namespace is // in status
{
constexpr UInt32 pin_mask_ejector = 1 << 14;
constexpr UInt32 pin_mask_height = 1 << 4;

using Ejector = Gpio<MappedAddress::gpio_0, pin_mask_ejector>;
using Height = Gpio<MappedAddress::gpio_0, pin_mask_height>;
} // namespace is

namespace oi // out interface
{
constexpr UInt32 pin_mask_q1 = 1 << 4;
constexpr UInt32 pin_mask_q2 = 1 << 5;
constexpr UInt32 pin_mask_reset = 1 << 3;
constexpr UInt32 pin_mask_start = 1 << 2;

using Q1 = Gpio<MappedAddress::gpio_2, pin_mask_q1>;
using Q2 = Gpio<MappedAddress::gpio_2, pin_mask_q2>;
using Reset = Gpio<MappedAddress::gpio_2, pin_mask_reset>;
using Start = Gpio<MappedAddress::gpio_2, pin_mask_start>;
} // namespace indicator

namespace op // out position
{
constexpr UInt32 pin_mask_ejector = 1 << 19;
constexpr UInt32 pin_mask_forward = 1 << 12;
constexpr UInt32 pin_mask_reverse = 1 << 13;
constexpr UInt32 pin_mask_slow = 1 << 14;
constexpr UInt32 pin_mask_stop = 1 << 15;

using Ejector = Gpio<MappedAddress::gpio_1, pin_mask_ejector>;
using Forward = Gpio<MappedAddress::gpio_1, pin_mask_forward>;
using Reverse = Gpio<MappedAddress::gpio_1, pin_mask_reverse>;
using Slow = Gpio<MappedAddress::gpio_1, pin_mask_slow>;
using Stop = Gpio<MappedAddress::gpio_1, pin_mask_stop>;
} // namespace op

namespace os // out status
{
constexpr UInt32 pin_mask_green = 1 << 18;
constexpr UInt32 pin_mask_red = 1 << 16;
constexpr UInt32 pin_mask_yellow = 1 << 17;

using Green = Gpio<MappedAddress::gpio_1, pin_mask_green>;
using Red = Gpio<MappedAddress::gpio_1, pin_mask_red>;
using Yellow = Gpio<MappedAddress::gpio_1, pin_mask_yellow>;
} // namespace os

} // namespace pal
} // namespace hal

#endif // HAL_PAL_HPP_
