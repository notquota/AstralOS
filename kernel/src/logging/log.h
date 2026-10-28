#pragma once

namespace Astral::Log {

void info(const char* message);
void success(const char* message);
void warning(const char* message);
void error(const char* message);
void download(const char* message);

}