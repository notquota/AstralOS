
#include "arp.h"
#include "ethernet.h"

namespace Astral::ARP {

static constexpr uint32_t CACHE_SIZE = 16;

struct Entry {
    bool used;
    Network::IPv4Address ip;
    Network::MacAddress mac;
};

static Entry cache[CACHE_SIZE]{};
static uint32_t nextReplacement = 0;

static constexpr Network::MacAddress BROADCAST = {{
    0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF
}};

void initialize() {
    for (uint32_t i = 0; i < CACHE_SIZE; ++i)
        cache[i] = {};

    nextReplacement = 0;
}

bool lookup(
    Network::IPv4Address ip,
    Network::MacAddress& mac
) {
    for (uint32_t i = 0; i < CACHE_SIZE; ++i) {
        if (cache[i].used &&
            Network::same_ip(cache[i].ip, ip)) {
            mac = cache[i].mac;
            return true;
        }
    }

    return false;
}

void remember(
    Network::IPv4Address ip,
    Network::MacAddress mac
) {
    if (Network::zero_ip(ip))
        return;

    // Reject multicast MAC addresses.
    if (mac.bytes[0] & 1)
        return;

    uint32_t index = CACHE_SIZE;

    for (uint32_t i = 0; i < CACHE_SIZE; ++i) {
        if (cache[i].used &&
            Network::same_ip(cache[i].ip, ip)) {
            index = i;
            break;
        }

        if (!cache[i].used && index == CACHE_SIZE)
            index = i;
    }

    if (index == CACHE_SIZE) {
        index = nextReplacement;
        nextReplacement =
            (nextReplacement + 1) % CACHE_SIZE;
    }

    cache[index].used = true;
    cache[index].ip = ip;
    cache[index].mac = mac;
}

bool request(Network::IPv4Address targetIP) {
    if (!Ethernet::initialized())
        return false;

    uint8_t packet[28]{};

    const auto localMac =
        Ethernet::local_mac();

    const auto localIP =
        Ethernet::local_ip();

    Network::write16(packet + 0, 1);
    Network::write16(packet + 2, 0x0800);

    packet[4] = 6;
    packet[5] = 4;

    Network::write16(packet + 6, 1);

    Network::copy(
        packet + 8,
        localMac.bytes,
        6
    );

    Network::copy(
        packet + 14,
        localIP.bytes,
        4
    );

    // Target hardware address is zero.
    Network::copy(
        packet + 24,
        targetIP.bytes,
        4
    );

    return Ethernet::send(
        BROADCAST,
        Ethernet::TYPE_ARP,
        packet,
        sizeof(packet)
    );
}

void receive(
    const uint8_t* packet,
    uint16_t length
) {
    if (!Ethernet::initialized() ||
        !packet ||
        length < 28)
        return;

    // Ethernet + IPv4 ARP only.
    if (Network::read16(packet + 0) != 1 ||
        Network::read16(packet + 2) != 0x0800 ||
        packet[4] != 6 ||
        packet[5] != 4)
        return;

    uint16_t operation =
        Network::read16(packet + 6);

    if (operation != 1 && operation != 2)
        return;

    Network::MacAddress senderMac{};
    Network::IPv4Address senderIP{};
    Network::IPv4Address targetIP{};

    Network::copy(
        senderMac.bytes,
        packet + 8,
        6
    );

    Network::copy(
        senderIP.bytes,
        packet + 14,
        4
    );

    Network::copy(
        targetIP.bytes,
        packet + 24,
        4
    );

    const auto localIP =
        Ethernet::local_ip();

    // Learn only messages directed to
    // our configured IPv4 address.
    if (!Network::same_ip(targetIP, localIP))
        return;

    remember(senderIP, senderMac);

    if (operation == 2)
        return;

    // Construct ARP reply.
    uint8_t reply[28]{};

    const auto localMac =
        Ethernet::local_mac();

    Network::write16(reply + 0, 1);
    Network::write16(reply + 2, 0x0800);

    reply[4] = 6;
    reply[5] = 4;

    Network::write16(reply + 6, 2);

    Network::copy(
        reply + 8,
        localMac.bytes,
        6
    );

    Network::copy(
        reply + 14,
        localIP.bytes,
        4
    );

    Network::copy(
        reply + 18,
        senderMac.bytes,
        6
    );

    Network::copy(
        reply + 24,
        senderIP.bytes,
        4
    );

    Ethernet::send(
        senderMac,
        Ethernet::TYPE_ARP,
        reply,
        sizeof(reply)
    );
}

}
