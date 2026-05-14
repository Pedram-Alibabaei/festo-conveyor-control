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
        virtual void init(ipc::AnlagenPosition pos) = 0;

        virtual void processEvent(Event e ) = 0;

        virtual void workpieceInfo(int id, WpType type) = 0;

        virtual void RequestToGiveWp() = 0;

        virtual void readyToReceiveWp() = 0;

        virtual void updateWpListe(wpType Type) = 0;

        virtual void tackt() = 0;



    };

} // namespace logik

#endif // I_LOGIK_HPP