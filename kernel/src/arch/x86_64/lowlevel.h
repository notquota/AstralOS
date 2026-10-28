#pragma once
#include <stdint.h>

extern "C" {

void astral_cli();
void astral_sti();
void astral_hlt();
void astral_pause();

uint64_t astral_read_cr0();
uint64_t astral_read_cr2();
uint64_t astral_read_cr3();

void astral_invalidate_page(void* address);

uint8_t astral_in8(uint16_t port);
void astral_out8(uint16_t port, uint8_t value);

}