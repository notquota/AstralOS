
#include "dhcp.h"
#include "udp.h"
#include "ethernet.h"
#include "ipv4.h"

namespace Astral::DHCP {

static constexpr Network::IPv4Address BROADCAST = {{
    255, 255, 255, 255
}};

static constexpr Network::IPv4Address ZERO = {{
    0, 0, 0, 0
}};

static constexpr uint16_t CLIENT_PORT = 68;
static constexpr uint16_t SERVER_PORT = 67;

static constexpr uint8_t DHCP_DISCOVER = 1;
static constexpr uint8_t DHCP_OFFER = 2;
static constexpr uint8_t DHCP_REQUEST = 3;
static constexpr uint8_t DHCP_ACK = 5;
static constexpr uint8_t DHCP_NAK = 6;

static State currentState = State::Idle;
static Lease currentLease{};

static uint32_t transactionId = 0;
static uint32_t retrySeconds = 0;
static uint32_t retryCount = 0;

static uint32_t leaseElapsed = 0;
static bool initializedFlag = false;

static void write32(uint8_t* p, uint32_t value) {
    p[0] = static_cast<uint8_t>(value >> 24);
    p[1] = static_cast<uint8_t>(value >> 16);
    p[2] = static_cast<uint8_t>(value >> 8);
    p[3] = static_cast<uint8_t>(value);
}

static uint32_t read32(const uint8_t* p) {
    return
        (static_cast<uint32_t>(p[0]) << 24) |
        (static_cast<uint32_t>(p[1]) << 16) |
        (static_cast<uint32_t>(p[2]) << 8) |
        static_cast<uint32_t>(p[3]);
}

static bool send_message(
    uint8_t messageType,
    bool includeRequest
) {
    if (!Ethernet::initialized())
        return false;

    uint8_t packet[300]{};

    packet[0] = 1; // BOOTREQUEST
    packet[1] = 1; // Ethernet
    packet[2] = 6; // MAC length

    write32(packet + 4, transactionId);

    // Request broadcast delivery.
    Network::write16(packet + 10, 0x8000);

    const auto mac = Ethernet::local_mac();

    Network::copy(packet + 28, mac.bytes, 6);

    // DHCP magic cookie.
    packet[236] = 99;
    packet[237] = 130;
    packet[238] = 83;
    packet[239] = 99;

    uint32_t offset = 240;

    packet[offset++] = 53;
    packet[offset++] = 1;
    packet[offset++] = messageType;

    // Client identifier: hardware type + MAC.
    packet[offset++] = 61;
    packet[offset++] = 7;
    packet[offset++] = 1;

    Network::copy(packet + offset, mac.bytes, 6);
    offset += 6;

    if (includeRequest) {
        // Requested IP.
        packet[offset++] = 50;
        packet[offset++] = 4;

        Network::copy(
            packet + offset,
            currentLease.address.bytes,
            4
        );

        offset += 4;

        // Server identifier.
        packet[offset++] = 54;
        packet[offset++] = 4;

        Network::copy(
            packet + offset,
            currentLease.server.bytes,
            4
        );

        offset += 4;
    }

    // Parameter request list.
    packet[offset++] = 55;
    packet[offset++] = 4;
    packet[offset++] = 1;  // Subnet mask
    packet[offset++] = 3;  // Router
    packet[offset++] = 6;  // DNS
    packet[offset++] = 51; // Lease lifetime

    packet[offset++] = 255;

    return UDP::send(
        BROADCAST,
        CLIENT_PORT,
        SERVER_PORT,
        packet,
        sizeof(packet)
    );
}

static void clear_address() {
    Ethernet::set_local_ip(ZERO);
    IPv4::configure(ZERO, ZERO);
}

static void handle_packet(
    Network::IPv4Address,
    uint16_t sourcePort,
    const uint8_t* data,
    uint16_t length
) {
    if (sourcePort != SERVER_PORT ||
        !data ||
        length < 240)
        return;

    if (currentState != State::Discovering &&
        currentState != State::Requesting)
        return;

    if (data[0] != 2 || data[1] != 1 || data[2] != 6)
        return;

    if (read32(data + 4) != transactionId)
        return;

    const auto mac = Ethernet::local_mac();

    if (!Network::equal(data + 28, mac.bytes, 6))
        return;

    const uint8_t cookie[4] = {99, 130, 83, 99};

    if (!Network::equal(data + 236, cookie, 4))
        return;

    Lease received{};
    uint8_t messageType = 0;

    Network::copy(
        received.address.bytes,
        data + 16,
        4
    );

    uint32_t offset = 240;

    while (offset < length) {
        const uint8_t option = data[offset++];

        if (option == 255)
            break;

        if (option == 0)
            continue;

        if (offset >= length)
            return;

        const uint8_t size = data[offset++];

        if (size > length - offset)
            return;

        const uint8_t* value = data + offset;

        switch (option) {
            case 53:
                if (size == 1)
                    messageType = value[0];
                break;

            case 1:
                if (size == 4)
                    Network::copy(
                        received.subnetMask.bytes,
                        value,
                        4
                    );
                break;

            case 3:
                if (size >= 4)
                    Network::copy(
                        received.gateway.bytes,
                        value,
                        4
                    );
                break;

            case 6:
                if (size >= 4)
                    Network::copy(
                        received.dnsServer.bytes,
                        value,
                        4
                    );
                break;

            case 54:
                if (size == 4)
                    Network::copy(
                        received.server.bytes,
                        value,
                        4
                    );
                break;

            case 51:
                if (size == 4)
                    received.leaseSeconds = read32(value);
                break;

            default:
                break;
        }

        offset += size;
    }

    if (currentState == State::Discovering &&
        messageType == DHCP_OFFER) {

        if (Network::zero_ip(received.address) ||
            Network::zero_ip(received.server))
            return;

        currentLease = received;
        currentState = State::Requesting;

        retrySeconds = 0;
        retryCount = 0;

        if (!send_message(DHCP_REQUEST, true))
            currentState = State::Failed;

        return;
    }

    if (currentState != State::Requesting)
        return;

    if (messageType == DHCP_NAK) {
        clear_address();
        currentState = State::Failed;
        return;
    }

    if (messageType != DHCP_ACK)
        return;

    if (!Network::same_ip(
        received.address,
        currentLease.address
    ))
        return;

    // An ACK can omit options sent in the OFFER.
    if (Network::zero_ip(received.server))
        received.server = currentLease.server;

    if (!Network::same_ip(
        received.server,
        currentLease.server
    ))
        return;

    if (Network::zero_ip(received.subnetMask))
        received.subnetMask = currentLease.subnetMask;

    if (Network::zero_ip(received.gateway))
        received.gateway = currentLease.gateway;

    if (Network::zero_ip(received.dnsServer))
        received.dnsServer = currentLease.dnsServer;

    if (received.leaseSeconds == 0)
        received.leaseSeconds =
            currentLease.leaseSeconds;

    if (Network::zero_ip(received.subnetMask) ||
        received.leaseSeconds == 0)
        return;

    currentLease = received;

    Ethernet::set_local_ip(currentLease.address);

    IPv4::configure(
        currentLease.subnetMask,
        currentLease.gateway
    );

    currentState = State::Bound;

    leaseElapsed = 0;
    retryCount = 0;
    retrySeconds = 0;
}

bool initialize() {
    currentState = State::Idle;
    currentLease = {};

    retryCount = 0;
    retrySeconds = 0;
    leaseElapsed = 0;
    initializedFlag = false;

    if (!Ethernet::initialized())
        return false;

    initializedFlag =
        UDP::bind(CLIENT_PORT, handle_packet);

    return initializedFlag;
}

bool start(uint32_t xid) {
    if (!initializedFlag || xid == 0)
        return false;

    transactionId = xid;

    currentLease = {};
    currentState = State::Discovering;

    retryCount = 0;
    retrySeconds = 0;
    leaseElapsed = 0;

    clear_address();

    if (!send_message(DHCP_DISCOVER, false)) {
        currentState = State::Failed;
        return false;
    }

    return true;
}

void tick() {
    if (currentState == State::Bound) {
        ++leaseElapsed;

        // Until lease renewal is implemented,
        // stop using an expired address.
        if (leaseElapsed >= currentLease.leaseSeconds) {
            clear_address();
            currentState = State::Failed;
        }

        return;
    }

    if (currentState != State::Discovering &&
        currentState != State::Requesting)
        return;

    if (++retrySeconds < 4)
        return;

    retrySeconds = 0;

    if (++retryCount >= 5) {
        clear_address();
        currentState = State::Failed;
        return;
    }

    const bool discovering =
        currentState == State::Discovering;

    send_message(
        discovering ? DHCP_DISCOVER : DHCP_REQUEST,
        !discovering
    );
}

State state() {
    return currentState;
}

bool configured() {
    return currentState == State::Bound;
}

const Lease& lease() {
    return currentLease;
}

}
