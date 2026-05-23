#ifndef LOGIK_HPP
#define LOGIK_HPP

#include "i_logik.h"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

class BaseState; // Forward declare BaseState

class Logik : public i_Logik {
private:
    ipc::AnlagenPosition m_position;
    I_Dispatcher* m_dispatcher;
    BaseState* m_currentState;

public:
    Logik();
    virtual ~Logik();

    void setDispatcher(I_Dispatcher* dispatcher);
    I_Dispatcher* getDispatcher() const { return m_dispatcher; }

    void transitionTo(BaseState* state);

        // i_Logik Interface
        virtual void init(ipc::AnlagenPosition pos) override;
        virtual void processEvent(Event e) override;
        virtual void workpieceInfo(int id, WpType type) override;
        virtual void RequestToGiveWp() override;
        virtual void readyToReceiveWp() override;
        virtual void updateWpListe(WpType Type) override;
        virtual void tackt() override;
    };

} // namespace logik

#endif // LOGIK_HPP
