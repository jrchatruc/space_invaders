#include <SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "machine.h"
#include "disassembler_8080.h"
#include "renderer.h"

#define FPS 59.541985
#define CLOCK_SPEED 1996800
#define CYCLES_PER_FRAME (CLOCK_SPEED / FPS)
#define VIDEO_BITMAP_START 0x2400;
static uint32_t current_time = 0;
static uint32_t last_time = 0;
static uint32_t dt = 0;

int main(int argc, char **argv)
{
    if ((argc > 2) && (strcmp(argv[1], "--disassemble") == 0))
    {
        disassemble_8080(argv[2]);
    }
    else
    {
        Machine *machine = init_machine();
        read_file_into_memory_at(machine, "game_files/invaders.h", 0);
        read_file_into_memory_at(machine, "game_files/invaders.g", 0x800);
        read_file_into_memory_at(machine, "game_files/invaders.f", 0x1000);
        read_file_into_memory_at(machine, "game_files/invaders.e", 0x1800);

        machine->cpu->write_byte = machine_write_byte;
        machine->cpu->port_input = machine_in;
        machine->cpu->port_output = machine_out;
        machine->cpu->user_data = machine;

        int which_interrupt = 1;
        SDL_Event event;
        bool quit = false;

        uint8_t *bitmap_buffer = machine->cpu->memory + VIDEO_BITMAP_START;

        if (!window_init())
        {
            printf("Failed to initialize!\n");
            exit(1);
        }

        while (!quit)
        {
            current_time = SDL_GetTicks();
            dt = current_time - last_time;

            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    quit = true;
                }
                if (event.type == SDL_KEYDOWN)
                {
                    machine_handle_key_down(machine, event.key.keysym.sym);
                }
                else if (event.type == SDL_KEYUP)
                {
                    machine_handle_key_up(machine, event.key.keysym.sym);
                }
            }

            uint32_t count = 0;
            State8080 *cpu = machine->cpu;
            while (count < dt * (CLOCK_SPEED / 1000))
            {
                int cycles = cpu->cycle_count;
                emulate_8080_op(cpu);
                int elapsed = cpu->cycle_count - cycles;
                count += elapsed;

                if (cpu->cycle_count >= CYCLES_PER_FRAME / 2)
                {
                    cpu->cycle_count -= CYCLES_PER_FRAME / 2;

                    generate_interrupt(cpu, which_interrupt);
                    if (which_interrupt == 1)
                    {
                        which_interrupt = 2;
                    }
                    else
                    {
                        draw_screen(bitmap_buffer);
                        which_interrupt = 1;
                    }
                }
            }

            last_time = current_time;
        }
        window_close();
    }
    return 0;
}
