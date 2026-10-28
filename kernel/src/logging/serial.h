#pragma once

namespace Astral::Serial {

void initialize();
bool available();

void put(char character);
void write(const char* message);

}