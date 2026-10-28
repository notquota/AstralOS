#include "log.h"
#include "../graphics/console.h"
#include "../graphics/theme.h"
#include "serial.h"

namespace Astral::Log {

static void output(
    const char* prefix,
    const char* message,
    uint32_t color
) {
    Serial::write(prefix);
    Serial::write(" ");
    Serial::write(message);
    Serial::write("\n");
    Console::write(prefix, color);
    Console::write(" ", Theme::TEXT);
    Console::write(message, Theme::TEXT);
    Console::newline();
}

void info(const char* message) {
    output("[INFO]", message, Theme::TEXT);
}

void success(const char* message) {
    output("[SUCCESS]", message, Theme::SUCCESS);
}

void warning(const char* message) {
    output("[WARNING]", message, Theme::WARNING);
}

void error(const char* message) {
    output("[ERROR]", message, Theme::ERROR);
}

void download(const char* message) {
    output("[DOWNLOAD]", message, Theme::DOWNLOAD);
}

}