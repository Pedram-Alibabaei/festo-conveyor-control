

#ifndef HAL_PAL_MAPPED_ADDRESS_HPP_
#define HAL_PAL_MAPPED_ADDRESS_HPP_

namespace hal
{
namespace pal
{

enum class MappedAddress
{
    null = 0x0,

    gpio_0 = 0x44E07000,
    gpio_1 = 0x4804C000,
    gpio_2 = 0x481AC000,

    adc_tsc = 0x44E0D000,
};

} // namespace pal
} // namespace hal

#endif // HAL_PAL_MAPPED_ADDRESS_HPP_
