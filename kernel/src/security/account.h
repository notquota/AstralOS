
#pragma once

#include <stdint.h>

namespace Astral::Security {

bool root_exists();

bool create_root(
    const char* owner,
    const char* password
);

bool authenticate_root(const char* password);

const char* device_owner();

}
