
#include "dns.h"
#include "udp.h"

namespace Astral::DNS {

static constexpr uint16_t LOCAL_PORT = 53053;
static constexpr uint16_t SERVER_PORT = 53;

static constexpr uint16_t MAX_PACKET = 512;

static Network::IPv4Address dnsServer{};
static Network::IPv4Address resolvedAddress{};

static State currentState = State::Idle;

static uint16_t currentId = 0;
static uint32_t elapsed = 0;

static uint8_t requestPacket[MAX_PACKET]{};
static uint16_t requestLength = 0;

static bool ready = false;

static bool skip_name(
    const uint8_t* packet,
    uint16_t length,
    uint32_t& offset
) {
    uint32_t steps = 0;

    while (offset < length && steps++ < 128) {
        const uint8_t label = packet[offset++];

        if (label == 0)
            return true;

        if ((label & 0xC0) == 0xC0) {
            if (offset >= length)
                return false;

            const uint16_t pointer =
                static_cast<uint16_t>(
                    ((label & 0x3F) << 8) |
                    packet[offset++]
                );

            return pointer < length;
        }

        if (label & 0xC0)
            return false;

        if (label > length - offset)
            return false;

        offset += label;
    }

    return false;
}

static void handle_response(
    Network::IPv4Address source,
    uint16_t sourcePort,
    const uint8_t* packet,
    uint16_t length
) {
    if (!ready ||
        currentState != State::Waiting ||
        sourcePort != SERVER_PORT ||
        !Network::same_ip(source, dnsServer) ||
        !packet ||
        length < 12)
        return;

    if (Network::read16(packet) != currentId)
        return;

    const uint16_t flags =
        Network::read16(packet + 2);

    // QR=1, opcode=0, TC=0, RCODE=0.
    if (!(flags & 0x8000) ||
        (flags & 0x7800) ||
        (flags & 0x0200) ||
        (flags & 0x000F))
        return;

    const uint16_t questionCount =
        Network::read16(packet + 4);

    const uint16_t answerCount =
        Network::read16(packet + 6);

    if (questionCount != 1)
        return;

    uint32_t offset = 12;

    if (!skip_name(packet, length, offset))
        return;

    if (offset + 4 > length)
        return;

    const uint16_t questionType =
        Network::read16(packet + offset);

    const uint16_t questionClass =
        Network::read16(packet + offset + 2);

    if (questionType != 1 || questionClass != 1)
        return;

    offset += 4;

    for (uint32_t i = 0; i < answerCount; ++i) {
        if (!skip_name(packet, length, offset))
            return;

        if (offset + 10 > length)
            return;

        const uint16_t type =
            Network::read16(packet + offset);

        const uint16_t recordClass =
            Network::read16(packet + offset + 2);

        const uint16_t dataLength =
            Network::read16(packet + offset + 8);

        offset += 10;

        if (dataLength > length - offset)
            return;

        if (type == 1 &&
            recordClass == 1 &&
            dataLength == 4) {

            Network::copy(
                resolvedAddress.bytes,
                packet + offset,
                4
            );

            currentState = State::Resolved;
            return;
        }

        offset += dataLength;
    }
}

bool initialize(Network::IPv4Address server) {
    ready = false;

    currentState = State::Idle;
    resolvedAddress = {};
    dnsServer = server;

    currentId = 0;
    elapsed = 0;
    requestLength = 0;

    if (Network::zero_ip(server))
        return false;

    ready = UDP::bind(
        LOCAL_PORT,
        handle_response
    );

    return ready;
}

static bool send_request() {
    if (!ready || requestLength == 0)
        return false;

    return UDP::send(
        dnsServer,
        LOCAL_PORT,
        SERVER_PORT,
        requestPacket,
        requestLength
    );
}

bool resolve(
    const char* hostname,
    uint16_t queryId
) {
    if (!ready || !hostname || !queryId)
        return false;

    currentState = State::Idle;
    resolvedAddress = {};
    currentId = queryId;
    elapsed = 0;
    requestLength = 0;

    for (uint32_t i = 0; i < MAX_PACKET; ++i)
        requestPacket[i] = 0;

    Network::write16(
        requestPacket + 0,
        queryId
    );

    // Standard recursive query.
    Network::write16(
        requestPacket + 2,
        0x0100
    );

    // One question.
    Network::write16(
        requestPacket + 4,
        1
    );

    uint32_t position = 12;
    const char* cursor = hostname;

    uint32_t nameLength = 0;

    while (*cursor) {
        if (++nameLength > 253)
            return false;

        if (position >= MAX_PACKET - 6)
            return false;

        const uint32_t lengthPosition =
            position++;

        uint32_t labelLength = 0;

        while (*cursor && *cursor != '.') {
            const char c = *cursor++;

            const bool validCharacter =
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '-';

            if (!validCharacter ||
                ++labelLength > 63 ||
                position >= MAX_PACKET - 6)
                return false;

            requestPacket[position++] =
                static_cast<uint8_t>(c);
        }

        if (labelLength == 0)
            return false;

        requestPacket[lengthPosition] =
            static_cast<uint8_t>(labelLength);

        if (*cursor == '.') {
            ++cursor;

            if (*cursor == '\0')
                break;
        }
    }

    if (nameLength == 0)
        return false;

    requestPacket[position++] = 0;

    // A record.
    Network::write16(
        requestPacket + position,
        1
    );
    position += 2;

    // Internet class.
    Network::write16(
        requestPacket + position,
        1
    );
    position += 2;

    requestLength =
        static_cast<uint16_t>(position);

    currentState = State::Waiting;

    // May initially fail while ARP is pending.
    // tick() retries after resolution.
    send_request();

    return true;
}

void tick() {
    if (currentState != State::Waiting)
        return;

    ++elapsed;

    if (elapsed == 2 || elapsed == 4)
        send_request();

    if (elapsed >= 7)
        currentState = State::Failed;
}

State state() {
    return currentState;
}

bool result(Network::IPv4Address& address) {
    if (currentState != State::Resolved)
        return false;

    address = resolvedAddress;
    return true;
}

}
