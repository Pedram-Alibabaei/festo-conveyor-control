/**
* @file ConnectionManager.hpp
 * @brief Schlanke Implementierung des Verbindungsmanagements.
 * * Der ConnectionManager findet den Partner und überwacht die Verbindung.
 * Er sendet selbst keine Datenpakete an den Partner.
 */

#ifndef CONNECTIONMANAGER_HPP
#define CONNECTIONMANAGER_HPP

#include "I_Connection.hpp"
#include <sys/dispatch.h>
#include <atomic>
#include <thread>
#include <shared/ipc_definitions.hpp>
#include "shared/types.hpp"

class ConnectionManager : public i_Connection {
private:
    int m_localCoid;
    std::atomic<int> m_partnerCoid;
    AnlagenPosition m_pos;

    /** Partner-HEARTBEAT seit letztem HEARTBEATTACKT empfangen? */
    std::atomic<bool> m_partnerAlive;
    std::atomic<bool> m_isConnected;
    std::atomic<int> m_connectGraceTicks;

    /** Anzahl aufeinanderfolgender Ticks ohne Partner-HEARTBEAT. */
    std::atomic<int> m_missedHeartbeats;

    static constexpr int kConnectGraceTicks = 6;
    static constexpr int kHeartbeatSendIntervalMs = 50;

    /** Erst nach so vielen verpassten Ticks wirklich trennen. Bei 50ms-Ticks
     *  ergibt das ~100ms Erkennungszeit und ist robust gegen den leichten
     *  Drift zwischen Sender-Thread (sleep_for) und Watchdog-Timer. */
    static constexpr int kMaxMissedHeartbeats = 2;

    std::thread m_connectThread;
    std::atomic<bool> m_isSearching;

    /** Eigener Thread fuer den HEARTBEAT-Versand an den Partner.
     *  Entkoppelt den (potenziell ueber qnet blockierenden) Send vom
     *  Dispatcher-Thread, damit der Watchdog-Tick nie eingefroren wird. */
    std::thread m_heartbeatThread;
    std::atomic<bool> m_heartbeatRunning;

    void connectionTask();
    void scheduleReconnect();
    void scheduleReconnectAsync();
    void notifyLocalDisconnect();
    void heartbeatSenderTask();
    void startHeartbeatSender();
    void stopHeartbeatSender();

public:
    ConnectionManager();
    virtual ~ConnectionManager();

    void init(int localCoid, AnlagenPosition pos) override;

    void connectToPartner() override;
    void heartbeatTackt() override;
    void forceDisconnect() override;
    void acknowledgeConnectionLost() override;
    void performReconnect() override;
    void partnerAlive() override;

    bool isConnected() const override;
    int getPartnerCoid() const override;
};

#endif
