#include <stdlib.h>
#include <stdint.h>
#include <SDL.h>
#include "machine.h"
#include "8080.h"

Machine *init_machine(void)
{
    State8080 *cpu = init_8080();
    Machine *machine = malloc(sizeof(Machine));
    machine->in_port_1 = 0x08;
    machine->in_port_2 = 0;

    machine->shift_high = 0;
    machine->shift_low = 0;
    machine->shift_offset = 0;
    machine->cpu = cpu;

    return machine;
}

void read_file_into_memory_at(Machine *machine, char *filename, uint32_t offset)
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL)
    {
        printf("error: Couldn't open %s\n", filename);
        exit(1);
    }

    fseek(f, 0L, SEEK_END);
    int fsize = ftell(f);
    fseek(f, 0L, SEEK_SET);

    uint8_t *buffer = &machine->cpu->memory[offset];
    fread(buffer, fsize, 1, f);
    fclose(f);
}

void machine_write_byte(void *data, uint16_t address, uint8_t value)
{
    State8080 *state = (State8080 *)data;

    if ((address >= 0x2000) && (address < 0x4000))
    {
        state->memory[address] = value;
    }
    else
    {
        fprintf(stderr, "Attempted to write to ROM\n");
        exit(1);
    }
}

uint8_t machine_in(void *data, uint8_t port_number)
{
    Machine *machine = (Machine *)data;
    uint8_t a = 0xff;

    switch (port_number)
    {
    case 0:
        break;
    case 1:
        a = machine->in_port_1;
        break;
    case 2:
        a = machine->in_port_2;
        break;
    case 3:
    {
        uint16_t shift = (machine->shift_high << 8) | machine->shift_low;
        a = (shift >> (8 - machine->shift_offset)) & 0xff;
        break;
    }
    default:
        fprintf(stderr, "error: unknown IN port %02x\n", port_number);
        exit(1);
        break;
    }

    return a;
}

void machine_out(void *data, uint8_t port_number, uint8_t value)
{
    Machine *machine = (Machine *)data;
    switch (port_number)
    {
    case 2:
        // The first three bits of the `a` register are stored into
        // the shift register offset.
        machine->shift_offset = value & 0x7;
        break;
    case 4:
        machine->shift_low = machine->shift_high;
        machine->shift_high = value;
        break;
    }
    return;
}

void machine_handle_key_down(Machine *machine, SDL_KeyCode key)
{
    switch (key)
    {
    case SDLK_LEFT:
        machine->in_port_1 |= 0x20; // Set bit 5 of port 1
        break;
    case SDLK_RIGHT:
        machine->in_port_1 |= 0x40; // Set bit 6 of port 1
        break;
    case SDLK_RETURN:
        machine->in_port_1 |= 0x4;
        break;
    case SDLK_SPACE:
        machine->in_port_1 |= 0x10;
        break;
    case SDLK_c:
        machine->in_port_1 |= 0x1;
        break;
    default:
        break;
    }
}

void machine_handle_key_up(Machine *machine, SDL_KeyCode key)
{
    switch (key)
    {
    case SDLK_LEFT:
        machine->in_port_1 &= ~0x20;
        break;
    case SDLK_RIGHT:
        machine->in_port_1 &= ~0x40;
        break;
    case SDLK_RETURN:
        machine->in_port_1 &= ~0x4;
        break;
    case SDLK_SPACE:
        machine->in_port_1 &= ~0x10;
        break;
    case SDLK_c:
        machine->in_port_1 &= ~0x1;
        break;
    default:
        break;
    }
}
