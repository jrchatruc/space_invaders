#pragma once
#include <stdint.h>
#include "8080.h"

typedef struct Machine
{
    uint8_t in_port_1, in_port_2;
    uint8_t out_port_3, out_port_5;
    uint8_t shift_high, shift_low, shift_offset;
    State8080 *cpu;
} Machine;

void machine_write_byte(void *data, uint16_t address, uint8_t value);
uint8_t machine_in(void *machine, uint8_t port_number);
void machine_out(void *machine, uint8_t port_number, uint8_t value);
void machine_handle_key_down(Machine *machine, SDL_KeyCode key);
void machine_handle_key_up(Machine *machine, SDL_KeyCode key);
void read_file_into_memory_at(Machine *machine, char *filename, uint32_t offset);
Machine *init_machine(void);
