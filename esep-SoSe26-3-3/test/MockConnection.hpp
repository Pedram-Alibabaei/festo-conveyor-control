#ifndef MOCK_CONNECTION_HPP
#define MOCK_CONNECTION_HPP

#include <gmock/gmock.h>
#include "connectionManager/i_Connection.hpp"

class MockConnection : public I_Connection {
public:
    MOCK_METHOD(void, initConnection, (int coid, ipc::AnlagenPosition pos), (override));
    MOCK_METHOD(void, connectToPartner, (), (override));
    MOCK_METHOD(void, heartbeatTackt, (), (override));
    MOCK_METHOD(void, forceDisconnect, (), (override));
    MOCK_METHOD(void, acknowledgeConnectionLost, (), (override));
    MOCK_METHOD(void, performReconnect, (), (override));
    MOCK_METHOD(void, partnerAlive, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(int, getPartnerCoid, (), (const, override));
};

#endif // MOCK_CONNECTION_HPP
