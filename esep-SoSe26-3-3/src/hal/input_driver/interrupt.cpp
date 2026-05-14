/*
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#include "interrupt.hpp"

#include <shared/types.hpp>

#include <chrono>
#include <system_error>
#include <thread>

extern "C"
{
#include <sys/neutrino.h>
#include <sys/procmgr.h>
}

using namespace hal::input_driver;

Interrupt::Interrupt() :
                m_running(false),
                m_interrupt_id(-1),
                m_channel_id(-1),
                m_connection_id(-1)
{
    setup_io_permissions();
    setup_channel();
}

Interrupt::~Interrupt() noexcept
{
    m_running = false;
    MsgSendPulse(m_connection_id, -1, pulse_stop_thread, 0);
    if (m_receiving_thread.joinable())
        m_receiving_thread.join();
    cleanup();
}

void
Interrupt::setup_io_permissions() const noexcept
{
    ThreadCtl(_NTO_TCTL_IO, 0);
    procmgr_ability(
            0,
            PROCMGR_ADN_ROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_INTERRUPT,
            PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_INTERRUPT,
            PROCMGR_ADN_ROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_IO,
            PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_IO,
            PROCMGR_AID_EOL);
    InterruptEnable();
}

void
Interrupt::setup_channel()
{
    m_channel_id = ChannelCreate(0);
    if (m_channel_id < 0)
    {
        throw std::system_error(errno, std::generic_category(),
                                "Could not create a channel!");
    }

    m_connection_id = ConnectAttach(0, 0, m_channel_id, _NTO_SIDE_CHANNEL, 0);
    if (m_connection_id < 0)
    {
        throw std::system_error(errno, std::generic_category(),
                                "Could not connect to channel!");
    }
}

int
Interrupt::setup_interrupt(SInt8 pulse, int intr) const
{
    struct sigevent intr_event;
    SIGEV_PULSE_INIT(&intr_event, m_connection_id, SIGEV_PULSE_PRIO_INHERIT,
                     pulse, 0);
    int interrupt_id = InterruptAttachEvent(intr, &intr_event, 0);
    if (interrupt_id < 0)
    {
        throw std::system_error(errno, std::generic_category(),
                                "Could not attach interrupt!");
    }
    return interrupt_id;
}

void
Interrupt::receive_loop()
{
    _pulse msg;
    while (m_running)
    {
        int recvid = MsgReceivePulse(m_channel_id, &msg, sizeof(_pulse),
                                     nullptr);
        if (recvid < 0)
        {
            throw std::system_error(errno, std::generic_category(),
                                    "MsgReceivePulse failed!");
        }
        else if (recvid == 0)
        {
            switch (msg.code)
            {
            case pulse_stop_thread:
                m_running = false;
                break;
            case pulse_intr:
                handle_interrupt();
                break;
            }
        }
    }
}

void
Interrupt::cleanup() const noexcept
{
    InterruptDetach(m_interrupt_id);
    ConnectDetach(m_connection_id);
    ChannelDestroy(m_channel_id);
}
