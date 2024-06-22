#include "gdbconnection.h"
#include "log.h"

#include <vdb.h>

#include <psp2/net/net.h>
#include <psp2/kernel/error.h>

// TODO: add to SDK
#include <psp2/kernel/threadmgr.h>
extern "C" int sceKernelOpenMsgPipe(const char *name);

GdbConnection::GdbConnection(int socket)
: m_socket(socket)
, m_valid(true)
, m_rxThread(this)
, m_txThread(this)
{
    LOG("GdbConnection ctor\n");

    auto nonblocking = 0;
    auto res = sceNetSetsockopt(socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &nonblocking, sizeof(nonblocking));

    if (res < 0)
    {
        LOG("error setting socket to blocking I/O (0x%08X)\n", res);
        return;
    }

    LOG("sceNetSetsockopt deactivate nonblocking: %08X\n", res);

    auto set = 1;
    res = sceNetSetsockopt(socket, SCE_NET_SOL_SOCKET, SCE_NET_TCP_NODELAY, &set, sizeof(set));

    if (res < 0)
    {
        LOG("error setting TCP no-delay on socket (0x%08X)\n", res);
    }

    LOG("sceNetSetsockopt activate tcp nodelay: %08X\n", res);

    // start our threads
    m_rxThread.start();
    m_txThread.start();
}

GdbConnection::~GdbConnection()
{
    close();
}

bool GdbConnection::valid() const
{
    return m_valid;
}

int GdbConnection::recv(char *data, unsigned int length) const
{
    return sceNetRecv(m_socket, data, length, 0);
}

int GdbConnection::send(const char *data, unsigned int length) const
{
    return sceNetSend(m_socket, data, length, 0);
}

int GdbConnection::close()
{
    auto res = sceNetSocketClose(m_socket);

    LOG("sceNetSocketClose: %08X\n", res);
    m_socket = -1;
    return res;
}

GdbConnection::RxThread::RxThread(GdbConnection *connection)
: Thread("tcp-rx-thread")
, m_connection(connection)
{
    setStackSize(0x3000);
}

GdbConnection::RxThread::~RxThread()
{
    int res = 0;
    res = sceKernelWaitThreadEnd(getThreadId(), nullptr, nullptr);

    LOG("wait for rx thread: 0x%08X\n", res);

    res = sceKernelDeleteThread(getThreadId());

    LOG("terminate rx thread: 0x%08X\n", res);
}

void GdbConnection::RxThread::run()
{
    LOG("start rx thread\n");

    while (m_connection->valid())
    {
        char data[0x2000] = {};
        auto res = m_connection->recv(data, sizeof(data));

        if (res < 0)
        {
            LOG("error receiving data from TCP: 0x%08X\n", res);
            vdb_send_serial_pipe("$k#6b", 5);
            break;
        }
        else if (res == 0)
        {
            break;
        }

        //LOG("data received: %s\n", data);
        res = vdb_send_serial_pipe(data, res);

        if (res < 0)
        {
            LOG("error sending data through pipe: 0x%08X\n", res);
            break;
        }
    }

    LOG("end rx thread\n");

    m_connection->m_valid = false;
}

GdbConnection::TxThread::TxThread(GdbConnection *connection)
    : Thread("tcp-tx-thread")
    , m_connection(connection)
{
    setStackSize(0x3000);
}

GdbConnection::TxThread::~TxThread()
{
    int res = 0;
    res = sceKernelWaitThreadEnd(getThreadId(), nullptr, nullptr);
    
    LOG("wait for tx thread: 0x%08X\n", res);

    res = sceKernelDeleteThread(getThreadId());

    LOG("terminate tx thread: 0x%08X\n", res);
}

void GdbConnection::TxThread::run()
{
    LOG("start tx thread\n");

    while (m_connection->valid())
    {
        unsigned int size = 0;
        int res = 0;
        char data[0x2000] = {};

        auto timeout = 100 * 1000u; // us

        // receive data from the kernel
        res = vdb_recv_serial_pipe(data, sizeof(data), timeout);

        if (res < 0)
        {
            if (static_cast<unsigned int>(res) == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
            {
                //LOG("wait for data timed out 0x%08X\n", res);
                continue;
            }

            LOG("error receiving data from the kernel 0x%08X\n", res);
            break;
        }

        size = res;
        //LOG("data sent: %s\n", data);
        res = m_connection->send(data, size);

        if (res < 0)
        {
            LOG("error sending data over TCP 0x%08X\n", res);
            break;
        }
    }

    LOG("end tx thread\n");

     m_connection->m_valid = false;
}
