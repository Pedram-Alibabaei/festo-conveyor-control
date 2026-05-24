#ifndef I_LOGIK_HPP
#define I_LOGIK_HPP

#include "shared/logikEvent.hpp"
#include "shared/types.hpp"

class i_Logik {
public:
    virtual ~i_Logik() = default;

    /**
     * @brief Nimmt ein Ereignis vom Dispatcher entgegen und triggert den Zustandsautomaten.
     * @param e Das aufgetretene Ereignis.
     * @param payload Optionaler Wert (z.B. der gemessene ADC-Wert der Höhe, Standard: 0).
     */
    virtual void init(AnlagenPosition pos) = 0;

    virtual void processEvent(logik::Event e ) = 0;

    virtual void workpieceTypeFromHS(WpType type) = 0;

    virtual void workpieceInfo(int id, WpType type) = 0;

    virtual void updateWpListe(WpType Type) = 0;

    virtual void tackt() = 0;



};



#endif // I_LOGIK_HPP
