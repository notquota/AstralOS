#include <stdint.h>

extern "C" {

int __cxa_atexit(
    void (*)(void*),
    void*,
    void*
) {
    return 0;
}

[[noreturn]]
void __cxa_pure_virtual() {
    asm volatile("cli");

    while (true)
        asm volatile("hlt");
}

[[noreturn]]
void __cxa_deleted_virtual() {
    asm volatile("cli");

    while (true)
        asm volatile("hlt");
}

void* __dso_handle;

int __cxa_guard_acquire(uint64_t* guard) {
    return *reinterpret_cast<uint8_t*>(guard) == 0;
}

void __cxa_guard_release(uint64_t* guard) {
    *reinterpret_cast<uint8_t*>(guard) = 1;
}

}