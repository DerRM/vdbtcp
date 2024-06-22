#pragma once

#include "thread.h"
#include "network.h"
#include "log.h"

#include <psp2/kernel/threadmgr.h>

template <typename TConnection>
class Server : private Thread
{
public:
    Server();

    bool listen(int port);

private:
    int m_socket = -1;

    void onNewConnection(int socket);
    void run() override;
};

template <typename TConnection>
Server<TConnection>::Server()
{
    m_socket = sceNetSocket("vdbtcp-server", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);

    if (m_socket < 0)
    {
        LOG("failed to create server socket: 0x%08X\n", m_socket);
        return;
    }

    LOG("created socket: %08X\n", m_socket);

    auto nonblocking = 1;
    auto res = sceNetSetsockopt(m_socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &nonblocking, sizeof(nonblocking));

    if (res < 0)
    {
        LOG("failed to mark port as non-blocking I/O: 0x%08X\n", res);
        return;
    }

    auto reuse_port = 1;
    res = sceNetSetsockopt(m_socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_REUSEPORT, &reuse_port, sizeof(reuse_port));

    if (res < 0)
    {
        LOG("failed to mark port as reusable: 0x%08X\n", res);
        return;
    }
}

template <typename TConnection>
bool Server<TConnection>::listen(int port)
{
    SceNetSockaddrIn addr = { 0 };

    addr.sin_family = SCE_NET_AF_INET;
    addr.sin_port = sceNetHtons(port);
    addr.sin_addr.s_addr = 0;

    auto res = sceNetBind(m_socket, (const SceNetSockaddr *)&addr, sizeof(addr));

    if (res < 0)
    {
        LOG("failed to bind to port %d: 0x%08X\n", port, res);
        sceNetSocketClose(m_socket);
        return false;
    }

    LOG("sceNetBind: %08X\n", res);

    res = sceNetListen(m_socket, 1);

    if (res < 0)
    {
        LOG("failed to listen on port %d: 0x%08X\n", port, res);
        sceNetSocketClose(m_socket);
        return false;
    }

    LOG("sceNetListen: %08X\n", res);

    if (start() < 0)
    {
        LOG("could not start thread for listening\n");
        sceNetSocketClose(m_socket);
        return false;
    }

    res = sceKernelWaitThreadEnd(getThreadId(), nullptr, nullptr);
    if (res < 0)
    {
        LOG("failed wait for server thread to end: 0x%08X\n", res);
        sceNetSocketClose(m_socket);
        return false;
    }

    LOG("started Thread for listen\n");

    return true;
}

template <typename TConnection>
void Server<TConnection>::run()
{
    bool running = true;
    while (running)
    {
        SceNetSockaddr addr;
        unsigned int len = sizeof(addr);

        auto res = sceNetAccept(m_socket, &addr, &len);

        if (res < 0)
        {
            if (static_cast<unsigned>(res) != SCE_NET_ERROR_EAGAIN)
            {
                // an actual error occured
                LOG("sceNetAccept failed: %08X\n", res);
                break;
            }
        }
        else
        {
            LOG("sceNetAccept: %08X\n", res);
            onNewConnection(res);

            running = false;
        }

        // yield
        sceKernelDelayThread(100 * 1000);
    }

    LOG("server thread terminated\n");
}

template <typename TConnection>
void Server<TConnection>::onNewConnection(int socket)
{
    TConnection connection(socket);

    while (connection.valid())
    {
        // yield
        sceKernelDelayThread(10 * 1000);
    }
}
