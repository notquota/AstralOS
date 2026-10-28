
#include "icmp.h"
#include "ipv4.h"

namespace Astral::ICMP {

static constexpr uint8_t ECHO_REPLY = 0;
static constexpr uint8_t ECHO_REQUEST = 8;

static constexpr uint16_t MAX_ICMP_SIZE = 1480;

static Statistics counters{};

const Statistics& statistics() {
    return counters;
}

static void write_checksum(
    uint8_t* packet,
    uint16_t length
) {
    packet[2] = 0;
    packet[3] = 0;

    Network::write16(
        packet + 2,
        Network::checksum(packet, length)
    );
}

void receive(
    Network::IPv4Address source,
    const uint8_t* packet,
    uint16_t length
) {
    if (!packet ||
        length < 8 ||
        length > MAX_ICMP_SIZE) {
        ++counters.invalidPackets;
        return;
    }

    if (Network::checksum(packet, length) != 0) {
        ++counters.invalidPackets;
        return;
    }

    const uint8_t type = packet[0];
    const uint8_t code = packet[1];

    if (type == ECHO_REPLY && code == 0) {
        ++counters.repliesReceived;
        return;
    }

    if (type != ECHO_REQUEST || code != 0)
        return;

    ++counters.requestsReceived;

    // Keep the original identifier, sequence
    // number and payload, as required by ICMP.
    uint8_t reply[MAX_ICMP_SIZE]{};

    Network::copy(
        reply,
        packet,
        length
    );

    reply[0] = ECHO_REPLY;
    reply[1] = 0;

    write_checksum(reply, length);

    if (IPv4::send(
        source,
        IPv4::PROTOCOL_ICMP,
        reply,
        length
    )) {
        ++counters.repliesSent;
    }
}

bool echo_request(
    Network::IPv4Address destination,
    uint16_t identifier,
    uint16_t sequence
) {
    // Eight-byte ICMP header + 32-byte payload.
    uint8_t packet[40]{};

    packet[0] = ECHO_REQUEST;
    packet[1] = 0;

    Network::write16(
        packet + 4,
        identifier
    );

    Network::write16(
        packet + 6,
        sequence
    );

    // Recognizable test payload.
    static constexpr char testData[] =
        "ASTRALOS-NETWORK-TEST";

    for (uint32_t i = 0;
         i < sizeof(testData) - 1;
         ++i) {
        packet[8 + i] =
            static_cast<uint8_t>(testData[i]);
    }

    write_checksum(
        packet,
        sizeof(packet)
    );

    return IPv4::send(
        destination,
        IPv4::PROTOCOL_ICMP,
        packet,
        sizeof(packet)
    );
}

}
