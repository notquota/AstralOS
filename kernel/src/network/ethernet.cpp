
#include "ethernet.h"
#include "arp.h"
#include "ipv4.h"

#include "../drivers/network/e1000.h"

namespace Astral::Ethernet {

static bool ready = false;

static Network::MacAddress localMac{};
static Network::IPv4Address localIP{};

bool initialize(Network::IPv4Address ip) {
    ready = false;

    if (!E1000::running())
        return false;

    E1000::MacAddress hardware{};

    if (!E1000::mac_address(hardware))
        return false;

    Network::copy(localMac.bytes, hardware.bytes, 6);
    localIP = ip;

    ARP::initialize();

    ready = true;
    E1000::set_receive_callback(receive);

    return true;
}

bool initialized() {
    return ready;
}

Network::MacAddress local_mac() {
    return localMac;
}

Network::IPv4Address local_ip() {
    return localIP;
}

void set_local_ip(Network::IPv4Address ip) {
    localIP = ip;

    // Flush old link-layer address mappings when
    // the interface changes its IPv4 address.
    if (ready)
        ARP::initialize();
}

bool send(
    const Network::MacAddress& destination,
    uint16_t etherType,
    const uint8_t* payload,
    uint16_t length
) {
    if (!ready || !E1000::running())
        return false;

    if (length > MAX_PAYLOAD)
        return false;

    if (length != 0 && !payload)
        return false;

    uint8_t frame[HEADER_SIZE + MAX_PAYLOAD]{};

    Network::copy(frame, destination.bytes, 6);
    Network::copy(frame + 6, localMac.bytes, 6);

    Network::write16(frame + 12, etherType);

    if (length != 0)
        Network::copy(frame + HEADER_SIZE, payload, length);

    return E1000::send(
        frame,
        static_cast<uint16_t>(HEADER_SIZE + length)
    );
}

void receive(
    const uint8_t* frame,
    uint16_t length
) {
    if (!ready || !frame || length < HEADER_SIZE)
        return;

    static constexpr uint8_t broadcast[6] = {
        255, 255, 255, 255, 255, 255
    };

    if (!Network::equal(frame, localMac.bytes, 6) &&
        !Network::equal(frame, broadcast, 6))
        return;

    const uint16_t type = Network::read16(frame + 12);

    const uint8_t* payload = frame + HEADER_SIZE;
    const uint16_t payloadLength =
        static_cast<uint16_t>(length - HEADER_SIZE);

    if (type == TYPE_ARP) {
        ARP::receive(payload, payloadLength);
        return;
    }

    if (type == TYPE_IPV4) {
        Network::MacAddress source{};

        Network::copy(source.bytes, frame + 6, 6);

        IPv4::receive(
            payload,
            payloadLength,
            source
        );
    }
}

}
