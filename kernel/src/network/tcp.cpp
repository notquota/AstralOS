
#include "tcp.h"

namespace Astral::TCP {

namespace {

constexpr uint16_t HEADER_SIZE = 20;
constexpr uint16_t MAX_PAYLOAD = 536;
constexpr uint16_t MAX_SEGMENT = HEADER_SIZE + MAX_PAYLOAD;
constexpr uint64_t RETRY_INTERVAL_MS = 900;
constexpr uint8_t MAX_RETRIES = 6;

constexpr uint8_t FLAG_FIN = 0x01;
constexpr uint8_t FLAG_SYN = 0x02;
constexpr uint8_t FLAG_RST = 0x04;
constexpr uint8_t FLAG_PSH = 0x08;
constexpr uint8_t FLAG_ACK = 0x10;

State currentState = State::Closed;

Network::IPv4Address localIP{};
Network::IPv4Address remoteIP{};

uint16_t localPort = 0;
uint16_t remotePort = 0;

uint32_t sequence = 0;
uint32_t expectedRemote = 0;

Transmit transmitter = nullptr;
Receive receiver = nullptr;

uint8_t pending[MAX_SEGMENT]{};

uint16_t pendingLength = 0;
uint32_t pendingEnd = 0;

uint8_t attempts = 0;
uint64_t lastTransmission = 0;

bool pendingValid = false;
bool timerArmed = false;

uint32_t read32(const uint8_t* source) {
    return
        (static_cast<uint32_t>(source[0]) << 24) |
        (static_cast<uint32_t>(source[1]) << 16) |
        (static_cast<uint32_t>(source[2]) << 8) |
        static_cast<uint32_t>(source[3]);
}

void write32(uint8_t* target, uint32_t value) {
    target[0] = static_cast<uint8_t>(value >> 24);
    target[1] = static_cast<uint8_t>(value >> 16);
    target[2] = static_cast<uint8_t>(value >> 8);
    target[3] = static_cast<uint8_t>(value);
}

uint16_t segment_checksum(
    Network::IPv4Address source,
    Network::IPv4Address destination,
    const uint8_t* segment,
    uint16_t length
) {
    uint32_t sum = 0;

    for (uint32_t i = 0; i < 4; i += 2) {
        sum +=
            (static_cast<uint16_t>(source.bytes[i]) << 8) |
            source.bytes[i + 1];

        sum +=
            (static_cast<uint16_t>(destination.bytes[i]) << 8) |
            destination.bytes[i + 1];
    }

    sum += 6;
    sum += length;

    for (uint32_t i = 0; i < length; i += 2) {
        uint16_t value =
            static_cast<uint16_t>(segment[i]) << 8;

        if (i + 1 < length)
            value |= segment[i + 1];

        sum += value;
    }

    while (sum >> 16)
        sum = (sum & 0xFFFFu) + (sum >> 16);

    return static_cast<uint16_t>(~sum);
}

bool emit(
    uint8_t flags,
    const uint8_t* payload,
    uint16_t size,
    bool track
) {
    if (!transmitter ||
        size > MAX_PAYLOAD ||
        (size && !payload))
        return false;

    uint8_t segment[MAX_SEGMENT]{};

    Network::write16(segment, localPort);
    Network::write16(segment + 2, remotePort);

    write32(segment + 4, sequence);
    write32(segment + 8, expectedRemote);

    segment[12] = 0x50;
    segment[13] = flags;

    Network::write16(segment + 14, 4096);

    for (uint32_t i = 0; i < size; ++i)
        segment[HEADER_SIZE + i] = payload[i];

    const uint16_t length =
        static_cast<uint16_t>(HEADER_SIZE + size);

    Network::write16(
        segment + 16,
        segment_checksum(
            localIP,
            remoteIP,
            segment,
            length
        )
    );

    if (!transmitter(remoteIP, segment, length))
        return false;

    if (track) {
        for (uint32_t i = 0; i < length; ++i)
            pending[i] = segment[i];

        pendingLength = length;

        pendingEnd =
            sequence +
            size +
            ((flags & FLAG_SYN) ? 1u : 0u) +
            ((flags & FLAG_FIN) ? 1u : 0u);

        sequence = pendingEnd;

        pendingValid = true;
        attempts = 0;
        timerArmed = false;
    }

    return true;
}

bool acknowledge() {
    return emit(
        FLAG_ACK,
        nullptr,
        0,
        false
    );
}

void reset() {
    currentState = State::Closed;

    localPort = 0;
    remotePort = 0;

    sequence = 0;
    expectedRemote = 0;

    pendingLength = 0;
    pendingEnd = 0;

    attempts = 0;
    lastTransmission = 0;

    pendingValid = false;
    timerArmed = false;

    remoteIP = {};
}

}

void initialize(
    Network::IPv4Address local,
    Transmit transmit
) {
    reset();

    localIP = local;
    transmitter = transmit;
    receiver = nullptr;
}

void set_receive_callback(Receive callback) {
    receiver = callback;
}

bool connect(
    Network::IPv4Address remote,
    uint16_t remotePortValue,
    uint16_t localPortValue,
    uint32_t initialSequence
) {
    if (currentState != State::Closed ||
        !transmitter ||
        Network::zero_ip(localIP) ||
        Network::zero_ip(remote) ||
        !remotePortValue ||
        !localPortValue)
        return false;

    remoteIP = remote;
    remotePort = remotePortValue;
    localPort = localPortValue;

    sequence = initialSequence;
    expectedRemote = 0;

    if (!emit(
        FLAG_SYN,
        nullptr,
        0,
        true
    )) {
        reset();
        return false;
    }

    currentState = State::SynSent;
    return true;
}

bool send(
    const uint8_t* data,
    uint16_t length
) {
    if (currentState != State::Established ||
        pendingValid ||
        !data ||
        length == 0 ||
        length > MAX_PAYLOAD)
        return false;

    return emit(
        static_cast<uint8_t>(FLAG_PSH | FLAG_ACK),
        data,
        length,
        true
    );
}

bool close() {
    if (currentState != State::Established ||
        pendingValid)
        return false;

    if (!emit(
        static_cast<uint8_t>(FLAG_FIN | FLAG_ACK),
        nullptr,
        0,
        true
    ))
        return false;

    currentState = State::Closing;
    return true;
}

void receive(
    Network::IPv4Address source,
    Network::IPv4Address destination,
    const uint8_t* segment,
    uint16_t length
) {
    if (currentState == State::Closed ||
        currentState == State::Error ||
        !segment ||
        length < HEADER_SIZE ||
        length > 1460 ||
        !Network::same_ip(source, remoteIP) ||
        !Network::same_ip(destination, localIP))
        return;

    const uint16_t fromPort =
        Network::read16(segment);

    const uint16_t toPort =
        Network::read16(segment + 2);

    if (fromPort != remotePort ||
        toPort != localPort ||
        (segment[12] >> 4) < 5 ||
        (segment[12] >> 4) * 4 > length ||
        segment_checksum(
            source,
            destination,
            segment,
            length
        ) != 0)
        return;

    const uint32_t remoteSequence =
        read32(segment + 4);

    const uint32_t acknowledged =
        read32(segment + 8);

    const uint8_t flags = segment[13];

    const uint16_t dataOffset =
        static_cast<uint16_t>(
            (segment[12] >> 4) * 4
        );

    const uint16_t dataLength =
        static_cast<uint16_t>(
            length - dataOffset
        );

    if (flags & FLAG_RST) {
        pendingValid = false;
        currentState = State::Error;
        return;
    }

    if (currentState == State::SynSent) {
        if ((flags & (FLAG_SYN | FLAG_ACK)) !=
                (FLAG_SYN | FLAG_ACK) ||
            !pendingValid ||
            acknowledged != pendingEnd)
            return;

        pendingValid = false;
        expectedRemote = remoteSequence + 1;
        currentState = State::Established;

        acknowledge();
        return;
    }

    if ((flags & FLAG_ACK) &&
        pendingValid &&
        acknowledged == pendingEnd) {
        pendingValid = false;
        timerArmed = false;

        if (currentState == State::Closing)
            currentState = State::Closed;
    }

    if (currentState == State::Closed)
        return;

    if (remoteSequence != expectedRemote) {
        acknowledge();
        return;
    }

    if (dataLength) {
        expectedRemote += dataLength;

        if (receiver)
            receiver(
                segment + dataOffset,
                dataLength
            );

        acknowledge();
    }

    if (flags & FLAG_FIN) {
        ++expectedRemote;
        acknowledge();

        pendingValid = false;
        currentState = State::Closed;
    }
}

void poll(uint64_t milliseconds) {
    if (!pendingValid ||
        !transmitter ||
        currentState == State::Error)
        return;

    if (!timerArmed) {
        lastTransmission = milliseconds;
        timerArmed = true;
        return;
    }

    if (milliseconds - lastTransmission <
        RETRY_INTERVAL_MS)
        return;

    if (attempts >= MAX_RETRIES) {
        currentState = State::Error;
        pendingValid = false;
        return;
    }

    if (transmitter(
        remoteIP,
        pending,
        pendingLength
    )) {
        ++attempts;
        lastTransmission = milliseconds;
    }
}

State state() {
    return currentState;
}

bool busy() {
    return pendingValid;
}

}