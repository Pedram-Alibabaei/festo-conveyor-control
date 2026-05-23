#ifndef BASE_STATE_HPP
#define BASE_STATE_HPP

#include "i_logik.h"

namespace logik {

class Logik; // Forward declaration

class BaseState {
protected:
    Logik* m_context;

public:
    virtual ~BaseState() = default;

    void setContext(Logik* context) {
        m_context = context;
    }

    // Haupteingang für alle Events, der von konkreten Zuständen überschrieben wird
    virtual void handle(Event e) = 0;
    
    // Lifecycle Hooks (optional, falls ein Zustand beim Betreten/Verlassen etwas tun muss)
    virtual void onEnter() {}
    virtual void onExit() {}
};

} // namespace logik

#endif // BASE_STATE_HPP
