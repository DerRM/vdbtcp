#include "gdbconnection.h"
#include "log.h"

#include <vdb.h>

#include "rsp.h"

#include <algorithm>

#include <psp2/net/net.h>
#include <psp2/kernel/error.h>

static constexpr char QSUPPORTED[]      = "qSupported";

#define STRLEN(str) sizeof(str) - 1

// TODO: add to SDK
#include <psp2/kernel/threadmgr.h>
extern "C" int sceKernelOpenMsgPipe(const char *name);

GdbConnection::GdbConnection(int socket)
: m_socket(socket)
, m_valid(true)
, m_gdbThread(this)
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
    m_gdbThread.start();
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

GdbConnection::GdbThread::GdbThread(GdbConnection *connection)
: Thread("tcp-gdb-thread")
, m_connection(connection)
{
    setStackSize(0x3000);
}

GdbConnection::GdbThread::~GdbThread()
{
    int res = 0;
    res = sceKernelWaitThreadEnd(getThreadId(), nullptr, nullptr);

    LOG("wait for gdb thread: 0x%08X\n", res);

    res = sceKernelDeleteThread(getThreadId());

    LOG("terminate gdb thread: 0x%08X\n", res);
}

static rsp::Packet processPacket(rsp::Packet& packet, rsp::Ringbuffer& output) {
    LOG("process packet\n");
    output.data[0] = '\n';
    output.length = 0;
    rsp::Packet out_packet;
    if (packet.length == 0) {
        return out_packet;
    }
    switch(packet.start[0]) {
        case 'q': {
            if (sceClibStrncmp(QSUPPORTED, packet.start, std::min(STRLEN(QSUPPORTED), packet.length)) == 0) {
                LOG("handle qSupported\n");
                out_packet.write("PacketSize=4096;qXfer:features:read+;swbreak+", output);
                LOG("output packet: %s\n", output.data);
            }
        } break;
    }

    return out_packet;
}

void GdbConnection::GdbThread::run()
{
    LOG("start gdb thread\n");

    rsp::Ringbuffer inputBuffer;
    rsp::Ringbuffer outputBuffer;

    while (m_connection->valid())
    {
        auto res = m_connection->recv(inputBuffer.data, sizeof(inputBuffer.data));

        if (res < 0)
        {
            LOG("error receiving data from TCP: 0x%08X\n", res);
            //vdb_send_serial_pipe("$k#6b", 5);
            break;
        }
        else if (res == 0)
        {
            break;
        }

        inputBuffer.length = static_cast<uint32_t>(res);
        inputBuffer.data[inputBuffer.length] = '\0';

        LOG("data received: %s\n", inputBuffer.data);
        rsp::Packet packet;
        packet.read(inputBuffer);

        LOG("read packet\n");

        res = m_connection->send(&packet.accept, 1);
        if (res < 0) {
            LOG("error sending data through TCP: 0x%08X\n", res);
            break;
        }

        LOG("packet response: %c\n", packet.accept);

        rsp::Packet output = processPacket(packet, outputBuffer);

        res = m_connection->send(outputBuffer.data, outputBuffer.length);
        if (res < 0) {
            LOG("error sending data through TCP: 0x%08X\n", res);
            break;
        }

        /*res = vdb_send_serial_pipe(data, res);

        if (res < 0)
        {
            LOG("error sending data through pipe: 0x%08X\n", res);
            break;
        }*/
    }

    LOG("end gdb thread\n");

    m_connection->m_valid = false;
}
