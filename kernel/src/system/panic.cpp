#include "panic.h"
#include "../graphics/framebuffer.h"
#include "../graphics/console.h"
#include "../graphics/theme.h"

namespace Astral {

[[noreturn]]
void kernel_panic(const char* reason) {
    Graphics::clear(0x200A18);

    Console::initialize();

    Graphics::clear(0x200A18);

    Console::write(
        "ASTRALOS - KERNEL PANIC",
        Theme::ERROR
    );

    Console::newline();

    Console::write(
        "A CRITICAL SYSTEM ERROR OCCURRED.",
        Theme::TEXT
    );

    Console::newline();

    Console::write(
        "REASON: ",
        Theme::WARNING
    );

    Console::write(
        reason ? reason : "UNKNOWN",
        Theme::TEXT
    );

    Console::newline();
    Console::newline();

    Console::write(
        "SYSTEM HALTED.",
        Theme::ERROR
    );

    // Interrupts must remain disabled here.
    asm volatile("cli" ::: "memory");

    while (true) {
        asm volatile("hlt");
    }
}

}