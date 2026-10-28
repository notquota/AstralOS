
#pragma once

#include <stdint.h>

namespace Astral::Network {

struct MacAddress {
    uint8_t bytes[6];
};

struct IPv4Address {
    uint8_t bytes[4];
};

inline uint16_t read16(const uint8_t* p) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(p[0]) << 8) |
        p[1]
    );
}

inline void write16(uint8_t* p, uint16_t value) {
    p[0] = static_cast<uint8_t>(value >> 8);
    p[1] = static_cast<uint8_t>(value);
}

inline void copy(
    uint8_t* dst,
    const uint8_t* src,
    uint32_t length
) {
    for (uint32_t i = 0; i < length; ++i)
        dst[i] = src[i];
}

inline bool equal(
    const uint8_t* a,
    const uint8_t* b,
    uint32_t length
) {
    for (uint32_t i = 0; i < length; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

inline bool same_ip(
    const IPv4Address& a,
    const IPv4Address& b
) {
    return equal(a.bytes, b.bytes, 4);
}

inline bool zero_ip(const IPv4Address& ip) {
    return ip.bytes[0] == 0 &&
           ip.bytes[1] == 0 &&
           ip.bytes[2] == 0 &&
           ip.bytes[3] == 0;
}

inline uint16_t checksum(
    const uint8_t* data,
    uint32_t length
) {
    uint32_t sum = 0;

    for (uint32_t i = 0; i < length; i += 2) {
        uint16_t word =
            static_cast<uint16_t>(data[i]) << 8;

        if (i + 1 < length)
            word |= data[i + 1];

        sum += word;
    }

    while (sum >> 16)
        sum = (sum & 0xFFFFu) + (sum >> 16);

    return static_cast<uint16_t>(~sum);
}

}
