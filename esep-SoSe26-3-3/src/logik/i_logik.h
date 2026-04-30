#ifndef I_LOGIK_HPP
#define I_LOGIK_HPP

#include "shared/logikEvents.hpp"

namespace logik {

    class i_Logik {
    public:
        virtual ~i_Logik() = default;

        /**
         * @brief Nimmt ein Ereignis vom Dispatcher entgegen und triggert den Zustandsautomaten.
         * @param e Das aufgetretene Ereignis.
         * @param payload Optionaler Wert (z.B. der gemessene ADC-Wert der Höhe, Standard: 0).
         */
        virtual void processEvent(Event e, int32_t payload = 0) = 0;

    };

} // namespace logik

#endif // I_LOGIK_HPP