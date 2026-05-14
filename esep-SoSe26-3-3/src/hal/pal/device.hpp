

#ifndef HAL_PAL_DEVICE_HPP_
#define HAL_PAL_DEVICE_HPP_

#include "mapped_address.hpp"
#include <shared/types.hpp>

#include <map>
#include <memory>
#include <mutex>

namespace hal
{
namespace pal
{

class Device
{
public:
    /**************************************************************************
     * @brief Wird von shared_ptr aufgerufen
     */
    Device() noexcept =default;

    /**************************************************************************
     * @brief Aufräumen, wenn keine Referenzen mehr auf den Speicher verweisen
     */
    ~Device() noexcept;

    /**************************************************************************
     * @brief Fabrikmethode zum Initialisieren einer Hardwareschnittstelle
     *
     * Beim ersten Aufruf auf eine Speicheradresse wird dieser Bereich in der
     * Hardware zugewiesen. Alle weiteren Aufrufe geben einen Zeiger auf das
     * bereits existierende Objekt zurück.
     *
     * @param address Startadresse des Blocks
     * @param length Länge des Blocks
     * @return Neues oder bereits existierendes Device
     */
    static std::shared_ptr<Device>
    map_memory(MappedAddress address, Size length) noexcept;

    /**************************************************************************
     * @brief Prüft, ob der gegebene Speicher bereits zugewiesen wurde
     *
     * @param address Startadresse des Blocks
     * @return Wahr, wenn ein Gerät diesem Speicher zugewiesen ist
     */
    static inline bool
    is_mapped(MappedAddress address)
    {
        return s_registered.addresses.find(address)
                != s_registered.addresses.end();
    }

    /**************************************************************************
     * @brief Ermöglicht den Zugriff auf das angeschlossene Gerät
     *
     * @return Vom System zugewiesene Speicheradresse
     */
    inline UIntPtr
    io_address() const noexcept
    {
        return m_io_address;
    }

private:
    static struct Registered
    {
        std::map<MappedAddress, std::weak_ptr<Device>> addresses;
        std::mutex mutex;
    } s_registered;

    MappedAddress m_address = MappedAddress::null;
    UIntPtr m_io_address = 0;
    Size m_length = 0;
};

} // namespace pal
} // namespace hal

#endif // HAL_PAL_DEVICE_HPP_
