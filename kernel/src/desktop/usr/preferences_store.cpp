
#include "preferences_store.h"
#include "../settings/registry.h"

namespace Astral::Desktop::PreferencesStore {

namespace {

constexpr char FILE_PATH[] =
    "/home/.config/astralos/desktop.settings";

constexpr uint32_t MAGIC = 0x41535046; // ASPF
constexpr uint32_t HEADER_BYTES = 16;
constexpr uint32_t VALUE_BYTES = 4;

ReadProvider readProvider = nullptr;
WriteProvider writeProvider = nullptr;

void put32(uint8_t* buffer, uint32_t value) {
    buffer[0] = static_cast<uint8_t>(value);
    buffer[1] = static_cast<uint8_t>(value >> 8);
    buffer[2] = static_cast<uint8_t>(value >> 16);
    buffer[3] = static_cast<uint8_t>(value >> 24);
}

uint32_t get32(const uint8_t* buffer) {
    return static_cast<uint32_t>(buffer[0]) |
           (static_cast<uint32_t>(buffer[1]) << 8) |
           (static_cast<uint32_t>(buffer[2]) << 16) |
           (static_cast<uint32_t>(buffer[3]) << 24);
}

uint32_t checksum(
    const uint8_t* data,
    uint32_t size
) {
    // FNV-1a: corruption detection, not authentication.
    uint32_t hash = 2166136261u;

    for (uint32_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }

    return hash;
}

}

void initialize() {
    readProvider = nullptr;
    writeProvider = nullptr;
}

void set_providers(
    ReadProvider reader,
    WriteProvider writer
) {
    readProvider = reader;
    writeProvider = writer;
}

bool persistence_available() {
    return readProvider && writeProvider;
}

const char* path() {
    return FILE_PATH;
}

bool save() {
    if (!writeProvider)
        return false;

    const uint32_t count = Settings::count();
    const uint32_t size =
        HEADER_BYTES + count * VALUE_BYTES;

    if (size > MAX_BYTES)
        return false;

    uint8_t buffer[MAX_BYTES]{};

    put32(buffer, MAGIC);
    put32(buffer + 4, FORMAT_VERSION);
    put32(buffer + 8, count);

    for (uint32_t i = 0; i < count; ++i) {
        const auto* item = Settings::at(i);

        if (!item)
            return false;

        put32(
            buffer + HEADER_BYTES + i * VALUE_BYTES,
            static_cast<uint32_t>(item->value)
        );
    }

    const uint32_t valueChecksum = checksum(
        buffer + HEADER_BYTES,
        count * VALUE_BYTES
    );

    put32(buffer + 12, valueChecksum);

    return writeProvider(FILE_PATH, buffer, size);
}

bool load() {
    if (!readProvider)
        return false;

    uint8_t buffer[MAX_BYTES]{};
    uint32_t bytesRead = 0;

    if (!readProvider(
        FILE_PATH, buffer, MAX_BYTES, bytesRead
    ))
        return false;

    if (bytesRead < HEADER_BYTES)
        return false;

    if (get32(buffer) != MAGIC ||
        get32(buffer + 4) != FORMAT_VERSION)
        return false;

    const uint32_t count = get32(buffer + 8);

    if (count != Settings::count())
        return false;

    const uint32_t required =
        HEADER_BYTES + count * VALUE_BYTES;

    if (bytesRead != required)
        return false;

    const uint32_t expected = get32(buffer + 12);
    const uint32_t actual = checksum(
        buffer + HEADER_BYTES,
        count * VALUE_BYTES
    );

    if (expected != actual)
        return false;

    for (uint32_t i = 0; i < count; ++i) {
        const int32_t value = static_cast<int32_t>(
            get32(
                buffer + HEADER_BYTES +
                i * VALUE_BYTES
            )
        );

        Settings::set(i + 1, value);
    }

    return true;
}

}
