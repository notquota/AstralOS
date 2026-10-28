
#include "ipv4.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"

namespace Astral::IPv4 {

static constexpr uint16_t HEADER_SIZE = 20;
static constexpr uint16_t MAX_DATA = 1480;

static Route currentRoute{};
static uint16_t nextIdentification = 1;

static constexpr Network::IPv4Address BROADCAST = {{
    255, 255, 255, 255
}};

static constexpr Network::MacAddress BROADCAST_MAC = {{
    255, 255, 255, 255, 255, 255
}};

void configure(
    Network::IPv4Address subnetMask,
    Network::IPv4Address gateway
) {
    currentRoute.subnetMask = subnetMask;
    currentRoute.gateway = gateway;
}

const Route& route() {
    return currentRoute;
}

static bool same_subnet(
    Network::IPv4Address a,
    Network::IPv4Address b,
    Network::IPv4Address mask
) {
    for (uint32_t i = 0; i < 4; ++i) {
        if ((a.bytes[i] & mask.bytes[i]) !=
            (b.bytes[i] & mask.bytes[i]))
            return false;
    }

    return true;
}

static bool resolve_mac(
    Network::IPv4Address destination,
    Network::MacAddress& mac
) {
    if (Network::same_ip(destination, BROADCAST)) {
        mac = BROADCAST_MAC;
        return true;
    }

    const Network::IPv4Address local =
        Ethernet::local_ip();

    if (Network::zero_ip(local))
        return false;

    Network::IPv4Address nextHop = destination;

    if (!same_subnet(
        local,
        destination,
        currentRoute.subnetMask
    )) {
        if (Network::zero_ip(currentRoute.gateway))
            return false;

        nextHop = currentRoute.gateway;
    }

    if (ARP::lookup(nextHop, mac))
        return true;

    // MAC resolution is asynchronous. Caller must
    // retry its packet once ARP has completed.
    ARP::request(nextHop);
    return false;
}

bool send(
    Network::IPv4Address destination,
    uint8_t protocol,
    const uint8_t* payload,
    uint16_t length
) {
    if (!Ethernet::initialized() || length > MAX_DATA)
        return false;

    if (length != 0 && !payload)
        return false;

    Network::MacAddress destinationMac{};

    if (!resolve_mac(destination, destinationMac))
        return false;

    uint8_t packet[HEADER_SIZE + MAX_DATA]{};

    packet[0] = 0x45;
    packet[1] = 0;

    Network::write16(
        packet + 2,
        static_cast<uint16_t>(HEADER_SIZE + length)
    );

    Network::write16(
        packet + 4,
        nextIdentification++
    );

    // Don't Fragment.
    Network::write16(packet + 6, 0x4000);

    packet[8] = 64;
    packet[9] = protocol;

    const Network::IPv4Address source =
        Ethernet::local_ip();

    Network::copy(packet + 12, source.bytes, 4);
    Network::copy(packet + 16, destination.bytes, 4);

    Network::write16(
        packet + 10,
        Network::checksum(packet, HEADER_SIZE)
    );

    if (length != 0)
        Network::copy(packet + HEADER_SIZE, payload, length);

    return Ethernet::send(
        destinationMac,
        Ethernet::TYPE_IPV4,
        packet,
        static_cast<uint16_t>(HEADER_SIZE + length)
    );
}

void receive(
    const uint8_t* packet,
    uint16_t length,
    Network::MacAddress sourceMac
) {
    if (!Ethernet::initialized() ||
        !packet ||
        length < HEADER_SIZE)
        return;

    if ((packet[0] >> 4) != 4)
        return;

    const uint16_t headerLength =
        static_cast<uint16_t>((packet[0] & 15) * 4);

    if (headerLength < HEADER_SIZE ||
        headerLength > length)
        return;

    const uint16_t totalLength =
        Network::read16(packet + 2);

    if (totalLength < headerLength ||
        totalLength > length)
        return;

    if (Network::checksum(packet, headerLength) != 0)
        return;

    // No fragmentation or reassembly yet.
    if ((Network::read16(packet + 6) & 0x3FFF) != 0)
        return;

    Network::IPv4Address source{};
    Network::IPv4Address destination{};

    Network::copy(source.bytes, packet + 12, 4);
    Network::copy(destination.bytes, packet + 16, 4);

    const bool localDestination =
        Network::same_ip(destination, Ethernet::local_ip());

    const bool broadcastDestination =
        Network::same_ip(destination, BROADCAST);

    if (!localDestination && !broadcastDestination)
        return;

    const uint8_t protocol = packet[9];

    if (protocol == PROTOCOL_ICMP && localDestination) {
        // Reject invalid ICMP sources.
        if (Network::zero_ip(source) ||
            source.bytes[0] >= 224)
            return;

        ARP::remember(source, sourceMac);

        ICMP::receive(
            source,
            packet + headerLength,
            static_cast<uint16_t>(totalLength - headerLength)
        );

        return;
    }

    if (protocol == PROTOCOL_UDP) {
        UDP::receive(
            source,
            destination,
            packet + headerLength,
            static_cast<uint16_t>(totalLength - headerLength)
        );
    }
}

}
