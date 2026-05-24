#ifndef MOCK_LOGIK_HPP
#define MOCK_LOGIK_HPP

#include <gmock/gmock.h>
#include "logik/i_logik.hpp"

class MockLogik : public logik::i_Logik {
public:
    MOCK_METHOD(void, init, (ipc::AnlagenPosition pos), (override));
    MOCK_METHOD(void, processEvent, (logik::Event e), (override));
    MOCK_METHOD(void, workpieceInfo, (int id, WpType type), (override));
    MOCK_METHOD(void, RequestToGiveWp, (), (override));
    MOCK_METHOD(void, readyToReceiveWp, (), (override));
    MOCK_METHOD(void, updateWpListe, (WpType Type), (override));
    MOCK_METHOD(void, tackt, (), (override));
};

#endif // MOCK_LOGIK_HPP
