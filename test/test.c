#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/8080.h"

#define MEMORY_SIZE 0x10000

static bool test_finished = 0;

static void write_byte(void *userdata, uint16_t address, uint8_t value)
{
    State8080 *const cpu = (State8080 *)userdata;
    cpu->memory[address] = value;
}

static uint8_t port_in(void *userdata, uint8_t port)
{
    return 0x00;
}

static void port_out(void *userdata, uint8_t port, uint8_t value)
{
    State8080 *const cpu = (State8080 *)userdata;

    if (port == 0)
    {
        test_finished = 1;
    }
    else if (port == 1)
    {
        uint8_t operation = cpu->c;

        if (operation == 2)
        { // print a character stored in E
            printf("%c", cpu->e);
        }
        else if (operation == 9)
        { // print from memory at (DE) until '$' char
            uint16_t addr = (cpu->d << 8) | cpu->e;
            do
            {
                printf("%c", cpu->memory[addr++]);
            } while (cpu->memory[addr] != '$');
        }
    }
}

static inline int load_file(const char *filename, State8080 *cpu, uint16_t addr)
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL)
    {
        fprintf(stderr, "error: can't open file '%s'.\n", filename);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    rewind(f);

    if (file_size + addr >= MEMORY_SIZE)
    {
        fprintf(stderr, "error: file %s can't fit in memory.\n", filename);
        return 1;
    }

    size_t result = fread(&cpu->memory[addr], sizeof(uint8_t), file_size, f);
    if (result != file_size)
    {
        fprintf(stderr, "error: while reading file '%s'\n", filename);
        return 1;
    }

    fclose(f);
    return 0;
}

static inline void run_test(const char *filename)
{
    State8080 *cpu = init_8080();
    cpu->user_data = cpu;
    cpu->write_byte = write_byte;
    cpu->port_input = port_in;
    cpu->port_output = port_out;
    memset(cpu->memory, 0, MEMORY_SIZE);

    if (load_file(filename, cpu, 0x100) != 0)
    {
        return;
    }
    printf("\n");
    printf("*** TEST: %s\n", filename);

    cpu->pc = 0x100;

    // inject "out 0,a" at 0x0000 (signal to stop the test)
    cpu->memory[0x0000] = 0xD3;
    cpu->memory[0x0001] = 0x00;

    // inject "out 1,a" at 0x0005 (signal to output some characters)
    cpu->memory[0x0005] = 0xD3;
    cpu->memory[0x0006] = 0x01;
    cpu->memory[0x0007] = 0xC9;

    test_finished = 0;
    while (!test_finished)
    {
        emulate_8080_op(cpu);
    }
}

int main(int argc, char **argv)
{
    run_test("./test/test_files/TST8080.COM");
    run_test("./test/test_files/CPUTEST.COM");
    run_test("./test/test_files/8080PRE.COM");
    run_test("./test/test_files/8080EXM.COM");

    return 0;
}
