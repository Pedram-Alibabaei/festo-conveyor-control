#ifndef BASE_STATE_HPP
#define BASE_STATE_HPP

#include "shared/logikEvent.hpp"

namespace logik {

class Logik;

class BaseState {
protected:
    Logik* m_context = nullptr;

public:
    virtual ~BaseState() = default;

    void setContext(Logik* context) {
        m_context = context;
    }

    virtual void handle(Event e) = 0;
    virtual void entry() {}
    virtual void exit() {}
};

} // namespace logik

#endif // BASE_STATE_HPP
