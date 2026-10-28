#pragma once

#include <stdint.h>

namespace Astral::Loading {

void begin(uint32_t totalTasks);
void complete(const char* stage, bool success);
bool finish();

}