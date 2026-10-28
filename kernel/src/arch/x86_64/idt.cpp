#include "idt.h"
#include "../../logging/serial.h"
#include "lowlevel.h"
#include <stdint.h>

namespace {

struct __attribute__((packed)) IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
};

struct __attribute__((packed)) IDTPointer {
    uint16_t limit;
    uint64_t base;
};

alignas(16) IDTEntry idt[256]{};

void set_gate(
    uint8_t vector,
    void (*handler)(),
    uint16_t selector
) {
    uint64_t address =
        reinterpret_cast<uint64_t>(handler);

    IDTEntry& gate = idt[vector];

    gate.offset_low = address & 0xFFFF;
    gate.selector = selector;
    gate.ist = 0;
    gate.attributes = 0x8E;
    gate.offset_mid = (address >> 16) & 0xFFFF;
    gate.offset_high =
        (address >> 32) & 0xFFFFFFFF;
    gate.reserved = 0;
}

void print_hex(uint64_t value) {
    const char digits[] = "0123456789ABCDEF";

    Astral::Serial::write("0x");

    for (int shift = 60; shift >= 0; shift -= 4) {
        char digit =
            digits[(value >> shift) & 0xF];

        Astral::Serial::put(digit);
    }
}

}

#define DECLARE_EXCEPTION(n) \
    extern "C" void astral_exception_##n();

DECLARE_EXCEPTION(0)
DECLARE_EXCEPTION(1)
DECLARE_EXCEPTION(2)
DECLARE_EXCEPTION(3)
DECLARE_EXCEPTION(4)
DECLARE_EXCEPTION(5)
DECLARE_EXCEPTION(6)
DECLARE_EXCEPTION(7)
DECLARE_EXCEPTION(8)
DECLARE_EXCEPTION(9)
DECLARE_EXCEPTION(10)
DECLARE_EXCEPTION(11)
DECLARE_EXCEPTION(12)
DECLARE_EXCEPTION(13)
DECLARE_EXCEPTION(14)
DECLARE_EXCEPTION(15)
DECLARE_EXCEPTION(16)
DECLARE_EXCEPTION(17)
DECLARE_EXCEPTION(18)
DECLARE_EXCEPTION(19)
DECLARE_EXCEPTION(20)
DECLARE_EXCEPTION(21)
DECLARE_EXCEPTION(22)
DECLARE_EXCEPTION(23)
DECLARE_EXCEPTION(24)
DECLARE_EXCEPTION(25)
DECLARE_EXCEPTION(26)
DECLARE_EXCEPTION(27)
DECLARE_EXCEPTION(28)
DECLARE_EXCEPTION(29)
DECLARE_EXCEPTION(30)
DECLARE_EXCEPTION(31)

#undef DECLARE_EXCEPTION

namespace Astral::CPU {

void initialize_idt() {
    astral_cli();

    void (*handlers[32])() = {
        astral_exception_0,
        astral_exception_1,
        astral_exception_2,
        astral_exception_3,
        astral_exception_4,
        astral_exception_5,
        astral_exception_6,
        astral_exception_7,
        astral_exception_8,
        astral_exception_9,
        astral_exception_10,
        astral_exception_11,
        astral_exception_12,
        astral_exception_13,
        astral_exception_14,
        astral_exception_15,
        astral_exception_16,
        astral_exception_17,
        astral_exception_18,
        astral_exception_19,
        astral_exception_20,
        astral_exception_21,
        astral_exception_22,
        astral_exception_23,
        astral_exception_24,
        astral_exception_25,
        astral_exception_26,
        astral_exception_27,
        astral_exception_28,
        astral_exception_29,
        astral_exception_30,
        astral_exception_31
    };

    uint16_t code_selector;

    asm volatile(
        "mov %%cs, %0"
        : "=r"(code_selector)
    );

    for (uint8_t i = 0; i < 32; ++i) {
        set_gate(i, handlers[i], code_selector);
    }

    IDTPointer pointer{
        static_cast<uint16_t>(
            sizeof(idt) - 1
        ),
        reinterpret_cast<uint64_t>(idt)
    };

    asm volatile(
        "lidt %0"
        :
        : "m"(pointer)
        : "memory"
    );
}

}

extern "C" [[noreturn]]
void astral_exception_dispatch(
    const Astral::CPU::ExceptionFrame* frame
) {
    using namespace Astral;

    astral_cli();

    Serial::write("\n");
    Serial::write("[ERROR] CPU EXCEPTION\n");

    Serial::write("Vector: ");
    print_hex(frame->vector);
    Serial::write("\n");

    Serial::write("Error code: ");
    print_hex(frame->error_code);
    Serial::write("\n");

    Serial::write("Instruction pointer: ");
    print_hex(frame->rip);
    Serial::write("\n");

    if (frame->vector == 14) {
        Serial::write("Page fault address: ");
        print_hex(astral_read_cr2());
        Serial::write("\n");
    }

    Serial::write("SYSTEM HALTED\n");

    while (true)
        astral_hlt();
}