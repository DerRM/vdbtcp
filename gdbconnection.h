#pragma once

#include "thread.h"

class GdbConnection
{
public:
    GdbConnection(int socket);
    ~GdbConnection();

    bool valid() const;

    int recv(char *data, unsigned int length) const;
    int send(const char *data, unsigned int length) const;
    int close();

private:

    class GdbThread : public Thread
    {
    public:
        GdbThread(GdbConnection *connection);
        ~GdbThread();

    private:
        void run() override;
        GdbConnection *m_connection = nullptr;
        //Ringbuffer m_inputBuffer;
        //Ringbuffer m_outputBuffer;
    };

    int m_socket = -1;
    bool m_valid = false;
    GdbThread m_gdbThread;
};
