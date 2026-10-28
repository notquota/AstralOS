
#include "udp.h"
#include "ipv4.h"
#include "ethernet.h"

namespace Astral::UDP {

static constexpr uint32_t MAX_BINDINGS = 16;
static constexpr uint16_t MAX_DATA = 1472;

struct Binding {
    uint16_t port;
    Handler handler;
};

static Binding bindings[MAX_BINDINGS]{};

static uint32_t add_words(
    uint32_t sum,
    const uint8_t* data,
    uint32_t length
) {
    for (uint32_t i = 0; i < length; i += 2) {
        uint16_t value =
            static_cast<uint16_t>(data[i]) << 8;

        if (i + 1 < length)
            value |= data[i + 1];

        sum += value;
    }

    return sum;
}

static uint16_t calculate_checksum(
    Network::IPv4Address source,
    Network::IPv4Address destination,
    const uint8_t* packet,
    uint16_t length
) {
    uint32_t sum = 0;

    sum = add_words(sum, source.bytes, 4);
    sum = add_words(sum, destination.bytes, 4);

    sum += IPv4::PROTOCOL_UDP;
    sum += length;

    sum = add_words(sum, packet, length);

    while (sum >> 16)
        sum = (sum & 0xFFFFu) + (sum >> 16);

    return static_cast<uint16_t>(~sum);
}

bool bind(uint16_t port, Handler handler) {
    if (!port || !handler)
        return false;

    for (auto& entry : bindings) {
        if (entry.port == port) {
            entry.handler = handler;
            return true;
        }
    }

    for (auto& entry : bindings) {
        if (entry.port == 0) {
            entry.port = port;
            entry.handler = handler;
            return true;
        }
    }

    return false;
}

void unbind(uint16_t port) {
    for (auto& entry : bindings) {
        if (entry.port == port) {
            entry = {};
            return;
        }
    }
}

bool send(
    Network::IPv4Address destination,
    uint16_t sourcePort,
    uint16_t destinationPort,
    const uint8_t* data,
    uint16_t length
) {
    if (!sourcePort || !destinationPort)
        return false;

    if (length > MAX_DATA ||
        (length && !data))
        return false;

    uint8_t packet[MAX_DATA + 8]{};

    Network::write16(packet + 0, sourcePort);
    Network::write16(packet + 2, destinationPort);
    Network::write16(
        packet + 4,
        static_cast<uint16_t>(length + 8)
    );

    if (length)
        Network::copy(packet + 8, data, length);

    uint16_t checksum = calculate_checksum(
        Ethernet::local_ip(),
        destination,
        packet,
        static_cast<uint16_t>(length + 8)
    );

    if (checksum == 0)
        checksum = 0xFFFF;

    Network::write16(packet + 6, checksum);

    return IPv4::send(
        destination,
        IPv4::PROTOCOL_UDP,
        packet,
        static_cast<uint16_t>(length + 8)
    );
}

void receive(
    Network::IPv4Address source,
    Network::IPv4Address destination,
    const uint8_t* packet,
    uint16_t length
) {
    if (!packet || length < 8)
        return;

    const uint16_t sourcePort =
        Network::read16(packet + 0);

    const uint16_t destinationPort =
        Network::read16(packet + 2);

    const uint16_t udpLength =
        Network::read16(packet + 4);

    if (udpLength < 8 || udpLength > length)
        return;

    const uint16_t transmittedChecksum =
        Network::read16(packet + 6);

    if (transmittedChecksum != 0 &&
        calculate_checksum(
            source,
            destination,
            packet,
            udpLength
        ) != 0)
        return;

    for (const auto& entry : bindings) {
        if (entry.port == destinationPort &&
            entry.handler) {
            entry.handler(
                source,
                sourcePort,
                packet + 8,
                static_cast<uint16_t>(udpLength - 8)
            );
            return;
        }
    }
}

}
