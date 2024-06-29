#pragma once

#include <cstdint>

namespace rsp
{
    struct Ringbuffer {
        char data[0x1000] = {};
        uint32_t length = 0;
    };

    struct Packet {
        char* start = nullptr;
        uint32_t length = 0;
        char accept = '-';

        void reset();
        int read(Ringbuffer const& buffer);
        int write(char const* output, Ringbuffer& outbuffer);
    };
}

inline void rsp::Packet::reset() {
    start = nullptr;
    length = 0;
}