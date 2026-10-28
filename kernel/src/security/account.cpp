
#include "account.h"

namespace Astral::Security {

static bool hasRoot = false;

static char ownerName[32] = {};
static char rootPassword[64] = {};

static void copy_text(
    char* destination,
    uint32_t capacity,
    const char* source
) {
    uint32_t i = 0;

    while (source[i] && i < capacity - 1) {
        destination[i] = source[i];
        ++i;
    }

    destination[i] = '\0';
}

static bool equal_text(
    const char* a,
    const char* b
) {
    while (*a && *b) {
        if (*a != *b)
            return false;

        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

bool root_exists() {
    return hasRoot;
}

bool create_root(
    const char* owner,
    const char* password
) {
    if (hasRoot || !owner || !password)
        return false;

    uint32_t ownerLength = 0;
    uint32_t passwordLength = 0;

    while (owner[ownerLength]) {
        if (++ownerLength >= sizeof(ownerName))
            return false;
    }

    while (password[passwordLength]) {
        if (++passwordLength >= sizeof(rootPassword))
            return false;
    }

    if (ownerLength == 0 || passwordLength < 8)
        return false;

    copy_text(ownerName, sizeof(ownerName), owner);
    copy_text(rootPassword, sizeof(rootPassword), password);

    hasRoot = true;
    return true;
}

bool authenticate_root(const char* password) {
    if (!hasRoot || !password)
        return false;

    return equal_text(password, rootPassword);
}

const char* device_owner() {
    return hasRoot ? ownerName : "UNCONFIGURED";
}

}
