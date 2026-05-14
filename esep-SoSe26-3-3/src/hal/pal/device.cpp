

#include "device.hpp"

#include <hal/pal/mapped_address.hpp>
#include <shared/types.hpp>

#include <map>
#include <memory>
#include <mutex>

extern "C"
{
#include <sys/mman.h>
}

using namespace hal::pal;

Device::Registered Device::s_registered { };

Device::~Device() noexcept
{
    std::lock_guard<std::mutex> lock(s_registered.mutex);

    const auto it = s_registered.addresses.find(m_address);
    if (it != s_registered.addresses.end() && it->second.expired())
    {
        munmap_device_io(m_io_address, m_length);
        s_registered.addresses.erase(it);
    }
}

std::shared_ptr<Device>
Device::map_memory(MappedAddress address, Size length) noexcept
{
    std::lock_guard<std::mutex> lock(s_registered.mutex);

    const auto it = s_registered.addresses.find(address);
    if (it != s_registered.addresses.end())
    {
        const auto instance = it->second.lock();
        if (instance)
        {
            return instance;
        }
    }

    const auto new_instance = std::make_shared<Device>();
    new_instance->m_address = address;
    new_instance->m_io_address = mmap_device_io(
            length, static_cast<UIntPtr>(address));
    new_instance->m_length = length;
    s_registered.addresses[address] = new_instance;

    return new_instance;
}

