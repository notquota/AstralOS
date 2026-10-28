
#include "acpi.h"

#include <stdint.h>

namespace Astral::ACPI {

static constexpr uint32_t MAX_TABLE_SIZE =
    1024u * 1024u;

static constexpr uint32_t MAX_TABLES = 256;

struct Rsdp {
    char signature[8];

    uint8_t checksum;
    char oemId[6];

    uint8_t revision;
    uint32_t rsdtAddress;

    uint32_t length;
    uint64_t xsdtAddress;

    uint8_t extendedChecksum;
    uint8_t reserved[3];
} __attribute__((packed));

static_assert(sizeof(Rsdp) == 36);

static State currentState{};
static MapPhysical physicalMapper = nullptr;

static const TableHeader* rootTable = nullptr;
static uint32_t rootLength = 0;

static bool same(
    const char* a,
    const char* b,
    uint32_t length
) {
    if (!a || !b)
        return false;

    for (uint32_t i = 0; i < length; ++i) {
        if (a[i] != b[i])
            return false;
    }

    return true;
}

static bool checksum_valid(
    const void* address,
    uint32_t length
) {
    if (!address || !length)
        return false;

    auto bytes =
        static_cast<const uint8_t*>(address);

    uint8_t sum = 0;

    for (uint32_t i = 0; i < length; ++i)
        sum = static_cast<uint8_t>(
            sum + bytes[i]
        );

    return sum == 0;
}

static const TableHeader* map_table(
    uint64_t physicalAddress
) {
    if (!physicalMapper ||
        !physicalAddress)
        return nullptr;

    // Map the fixed header first.
    const void* initial = physicalMapper(
        physicalAddress,
        sizeof(TableHeader)
    );

    if (!initial)
        return nullptr;

    auto header =
        static_cast<const TableHeader*>(initial);

    const uint32_t length = header->length;

    if (length < sizeof(TableHeader) ||
        length > MAX_TABLE_SIZE)
        return nullptr;

    // Map the complete table.
    const void* complete = physicalMapper(
        physicalAddress,
        length
    );

    if (!complete)
        return nullptr;

    if (!checksum_valid(complete, length))
        return nullptr;

    return static_cast<const TableHeader*>(
        complete
    );
}

static uint64_t entry_address(
    uint32_t index
) {
    if (!rootTable)
        return 0;

    const uint32_t entrySize =
        currentState.usingXsdt ? 8 : 4;

    const uint64_t offset =
        sizeof(TableHeader) +
        static_cast<uint64_t>(index) * entrySize;

    if (offset + entrySize > rootLength)
        return 0;

    auto bytes =
        reinterpret_cast<const uint8_t*>(
            rootTable
        ) + offset;

    // Construct the address byte by byte,
    // avoiding unaligned pointer loads.
    uint64_t result = 0;

    for (uint32_t i = 0; i < entrySize; ++i)
        result |=
            static_cast<uint64_t>(bytes[i])
            << (i * 8);

    return result;
}

bool initialize(
    const void* rsdpVirtualAddress,
    MapPhysical mapper
) {
    currentState = {};
    physicalMapper = nullptr;

    rootTable = nullptr;
    rootLength = 0;

    if (!rsdpVirtualAddress || !mapper)
        return false;

    // Every RSDP has a 20-byte ACPI 1.0 prefix.
    const auto* rsdp =
        static_cast<const Rsdp*>(
            rsdpVirtualAddress
        );

    if (!same(
        rsdp->signature,
        "RSD PTR ",
        8
    )) {
        return false;
    }

    if (!checksum_valid(rsdp, 20))
        return false;

    if (rsdp->revision >= 2) {
        if (rsdp->length < sizeof(Rsdp) ||
            rsdp->length > 4096)
            return false;

        if (!checksum_valid(
            rsdp,
            rsdp->length
        )) {
            return false;
        }
    }

    physicalMapper = mapper;

    uint64_t rootPhysical = 0;
    bool xsdt = false;

    if (rsdp->revision >= 2 &&
        rsdp->xsdtAddress != 0) {

        rootPhysical = rsdp->xsdtAddress;
        xsdt = true;
    } else {
        rootPhysical = rsdp->rsdtAddress;
    }

    const TableHeader* root =
        map_table(rootPhysical);

    if (!root) {
        physicalMapper = nullptr;
        return false;
    }

    const char* expected =
        xsdt ? "XSDT" : "RSDT";

    if (!same(
        root->signature,
        expected,
        4
    )) {
        physicalMapper = nullptr;
        return false;
    }

    const uint32_t entrySize = xsdt ? 8 : 4;
    const uint32_t payloadSize =
        root->length - sizeof(TableHeader);

    if (payloadSize % entrySize != 0) {
        physicalMapper = nullptr;
        return false;
    }

    rootTable = root;
    rootLength = root->length;

    currentState.initialized = true;
    currentState.usingXsdt = xsdt;
    currentState.revision = rsdp->revision;

    currentState.rootTablePhysical =
        rootPhysical;

    currentState.tableCount =
        payloadSize / entrySize;

    if (currentState.tableCount > MAX_TABLES)
        currentState.tableCount = MAX_TABLES;

    return true;
}

bool initialized() {
    return currentState.initialized;
}

const State& state() {
    return currentState;
}

bool find_table(
    const char* signature,
    const TableHeader*& table,
    uint32_t& length
) {
    table = nullptr;
    length = 0;

    if (!initialized() || !signature)
        return false;

    for (uint32_t i = 0;
         i < currentState.tableCount;
         ++i) {

        const uint64_t physical =
            entry_address(i);

        if (!physical)
            continue;

        const TableHeader* candidate =
            map_table(physical);

        if (!candidate)
            continue;

        if (!same(
            candidate->signature,
            signature,
            4
        )) {
            continue;
        }

        table = candidate;
        length = candidate->length;

        return true;
    }

    return false;
}

}
