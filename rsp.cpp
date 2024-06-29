#include "rsp.h"
#include "hex.h"
#include "log.h"

#include <psp2kern/kernel/threadmgr.h>

namespace
{
    static constexpr auto RSP_START_TOKEN      = '$';
    static constexpr auto RSP_END_TOKEN        = '#';
    static constexpr auto RSP_ESCAPE_TOKEN     = '}';
    static constexpr auto RSP_RUNLENGTH_TOKEN  = '*';
    static constexpr auto RSP_ACCEPT           = '+';
    static constexpr auto RSP_DECLINE          = '-';


    /*int unescape(int token)
    {
        if (token != RSP_ESCAPE_TOKEN)
        {
            return token;
        }

        return serial::get() ^ 0x20;
    }*/

    int escape(int ch)
    {
        return ch ^ 0x20;
    }

    int is_token(int ch)
    {
        return (ch == RSP_START_TOKEN
            || ch == RSP_END_TOKEN
            || ch == RSP_ESCAPE_TOKEN
            || ch == RSP_RUNLENGTH_TOKEN);
    }
}

namespace rsp
{
    int Packet::read(Ringbuffer const& buffer)
    {
        uint32_t pointer = 0;
        while (buffer.data[pointer] != RSP_START_TOKEN)
        {
            if (buffer.data[pointer] == '\0') {
                return -1;
            }

            // we need to yield so other services can run still
            //ksceKernelDelayThread(100 * 1000);
            ++pointer;
            if (pointer == buffer.length) {
                return -1;
            }
        }

        ++pointer;

        start = (char*)&buffer.data[pointer];

        uint32_t checksum = 0u;

        uint32_t size = 0;
        for (uint32_t i = pointer; i < buffer.length; ++i, ++size)
        {
            auto token = buffer.data[i];

            if (token == RSP_END_TOKEN)
            {
                this->length = size;
                auto upper = hex::from_char(buffer.data[++i]) << 4;
                auto lower = hex::from_char(buffer.data[++i]);

                // checksum is mod 256 of sum of all data
                if ((checksum & 0xFF) != (upper | lower))
                {
                    // TODO: handle error
                    LOG("rsp: bad checksum: 0x%02X != 0x%02X\n", checksum & 0xFF, upper | lower);
                    break;
                }

                // null terminate cstring
                //out[i] = '\0';
                //out.put(RSP_ACCEPT);

                LOG("rsp OK\n");
                accept = '+';

                return 0;
            }

            checksum += token;
            //out[i] = unescape(token);
        }

        //out.put(RSP_DECLINE);
        return -1;
    }

    int Packet::write(char const* output, Ringbuffer& outbuffer)
    {
        if (output == nullptr || output[0] == '\0') {
            return -1;
        }
        //LOG("rsp > (\"%s\"): 0x%08X\n", data, size);
        uint32_t size = 0;

        outbuffer.data[size++] = RSP_START_TOKEN;

        this->start = &outbuffer.data[size];

        auto checksum = 0u;

        uint32_t pointer = 0;

        while (output[pointer] != '\0')
        {
            auto ch = output[pointer];

            // check for any reserved tokens
            if (is_token(ch))
            {
                outbuffer.data[size++] = RSP_ESCAPE_TOKEN;
                checksum += RSP_ESCAPE_TOKEN;
                ch = escape(ch);
            }

            outbuffer.data[size++] = ch;
            checksum += ch;
            ++pointer;
        }

        this->length = pointer;

        outbuffer.data[size++] = RSP_END_TOKEN;

        // checksum is mod 256
        checksum &= 0xFF;
        outbuffer.data[size++] = hex::to_char((checksum >> 4) & 0xF);
        outbuffer.data[size++] = hex::to_char(checksum & 0xF);
        outbuffer.data[size] = '\0';
        outbuffer.length = size;
        return 0;
    }
}