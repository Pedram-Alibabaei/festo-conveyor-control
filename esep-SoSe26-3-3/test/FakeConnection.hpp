#ifndef FAKE_CONNECTION_HPP
#define FAKE_CONNECTION_HPP

#include "connectionManager/i_Connection.hpp"

class FakeConnection : public i_Connection {
public:
    int heartbeatTicks = 0;
    int partnerAliveTicks = 0;
    bool connected = true;
    int partnerCoid = 42;

    void init(int, AnlagenPosition) override {}

    void connectToPartner() override {}

    void heartbeatTackt() override {
        ++heartbeatTicks;
    }

    void partnerAlive() override {
        ++partnerAliveTicks;
    }

    bool isConnected() const override {
        return connected;
    }

    int getPartnerCoid() const override {
        return partnerCoid;
    }

    void forceDisconnect() override {}

    void acknowledgeConnectionLost() override {
        connected = false;
    }

    void performReconnect() override {}
};

#endif // FAKE_CONNECTION_HPP
