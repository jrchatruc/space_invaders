#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "8080.h"
#include "disassembler_8080.h"

// clang-format off
unsigned char cycles8080[] = {
	4, 10, 7, 5, 5, 5, 7, 4, 4, 10, 7, 5, 5, 5, 7, 4, //0x00..0x0f
	4, 10, 7, 5, 5, 5, 7, 4, 4, 10, 7, 5, 5, 5, 7, 4, //0x10..0x1f
	4, 10, 16, 5, 5, 5, 7, 4, 4, 10, 16, 5, 5, 5, 7, 4, //etc
	4, 10, 13, 5, 10, 10, 10, 4, 4, 10, 13, 5, 5, 5, 7, 4,

	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5, //0x40..0x4f
	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
	5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
	7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 5, 5, 5, 5, 7, 5,

	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, //0x80..8x4f
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,

	11, 10, 10, 10, 17, 11, 7, 11, 11, 10, 10, 10, 10, 17, 7, 11, //0xc0..0xcf
	11, 10, 10, 10, 17, 11, 7, 11, 11, 10, 10, 10, 10, 17, 7, 11,
	11, 10, 10, 18, 17, 11, 7, 11, 11, 5, 10, 5, 17, 17, 7, 11,
	11, 10, 10, 4, 17, 11, 7, 11, 11, 5, 10, 4, 17, 17, 7, 11,
};
// clang-format on

static bool carry(int bit_number, uint8_t a, uint8_t b, bool cy)
{
    int16_t result = a + b + cy;
    int16_t carry = result ^ a ^ b;
    return carry & (1 << bit_number);
}

static int parity(int x, int size)
{
    int i;
    int p = 0;
    x = (x & ((1 << size) - 1));
    for (i = 0; i < size; i++)
    {
        if (x & 0x1)
            p++;
        x = x >> 1;
    }
    return (0 == (p & 0x1));
}

static void flags_zsp(State8080 *state, uint8_t value)
{
    state->cc.z = (value == 0);
    state->cc.s = value >> 7;
    state->cc.p = parity(value, 8);
}

static void write_mem(State8080 *state, uint16_t address, uint8_t value)
{
    state->write_byte(state, address, value);
}

static uint8_t read_from_m(State8080 *state)
{
    uint16_t offset = (state->h << 8) | state->l;
    return state->memory[offset];
}

static void write_to_m(State8080 *state, uint8_t value)
{
    uint16_t offset = (state->h << 8) | state->l;
    write_mem(state, offset, value);
}

static uint16_t get_bc(State8080 *state)
{
    return (state->b << 8) | state->c;
}

static uint16_t get_de(State8080 *state)
{
    return (state->d << 8) | state->e;
}

static uint16_t get_hl(State8080 *state)
{
    return (state->h << 8) | state->l;
}

static void set_bc(State8080 *state, uint16_t value)
{
    state->b = value >> 8;
    state->c = value & 0xff;
}

static void set_de(State8080 *state, uint16_t value)
{
    state->d = value >> 8;
    state->e = value & 0xff;
}

static void set_hl(State8080 *state, uint16_t value)
{
    state->h = value >> 8;
    state->l = value & 0xff;
}

static void add(State8080 *state, uint8_t *reg, uint8_t value, bool cy)
{
    uint8_t result = *reg + value + cy;
    state->cc.cy = carry(8, *reg, value, cy);
    state->cc.ac = carry(4, *reg, value, cy);
    flags_zsp(state, result);
    *reg = result;
}

static void substract(State8080 *state, uint8_t *const reg, uint8_t val, bool cy)
{
    add(state, reg, ~val, !cy);
    state->cc.cy = !state->cc.cy;
}

static int inr(State8080 *state, uint8_t value)
{
    uint8_t result = value + 1;
    state->cc.ac = (result & 0xf) == 0;
    flags_zsp(state, result);
    return result;
}

static int dcr(State8080 *state, uint8_t value)
{
    uint8_t result = value - 1;
    state->cc.ac = !((result & 0xf) == 0xf);
    flags_zsp(state, result);
    return result;
}

static void cmp(State8080 *state, uint8_t value)
{
    int16_t result = state->a - value;
    state->cc.cy = result >> 8;
    state->cc.ac = ~(state->a ^ result ^ value) & 0x10;
    flags_zsp(state, result & 0xff);
}

static void dad(State8080 *state, uint16_t value)
{
    state->cc.cy = ((get_hl(state) + value) >> 16) & 1;
    set_hl(state, get_hl(state) + value);
}

static void ana(State8080 *state, uint8_t value)
{
    uint8_t result = state->a & value;
    state->cc.cy = 0;
    state->cc.ac = ((state->a | value) & 0x08) != 0;
    flags_zsp(state, result);
    state->a = result;
}

static void xra(State8080 *state, uint8_t value)
{
    state->a ^= value;
    state->cc.cy = 0;
    state->cc.ac = 0;
    flags_zsp(state, state->a);
}

static void ora(State8080 *state, uint8_t value)
{
    state->a |= value;
    state->cc.cy = 0;
    state->cc.ac = 0;
    flags_zsp(state, state->a);
}

static void push(State8080 *state, uint8_t high, uint8_t low)
{
    write_mem(state, state->sp - 1, high);
    write_mem(state, state->sp - 2, low);
    state->sp -= 2;
}

static void ret(State8080 *state)
{
    state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
    state->sp += 2;
}

static void call(State8080 *state, uint16_t ret, uint16_t address)
{
    push(state, (ret >> 8) & 0xff, ret & 0xff);
    state->pc = address;
}

static void cond_call(State8080 *state, uint16_t address, bool condition)
{
    if (condition)
    {
        call(state, state->pc + 2, address);
    }
    else
    {
        state->pc += 2;
    }
}

static void cond_jump(State8080 *state, uint16_t address, bool condition)
{
    if (condition)
    {
        state->pc = address;
    }
    else
    {
        state->pc += 2;
    }
}

static void pop(State8080 *state, uint8_t *high, uint8_t *low)
{
    *high = state->memory[state->sp + 1];
    *low = state->memory[state->sp];
    state->sp += 2;
}

void generate_interrupt(State8080 *state, int interrupt_num)
{
    call(state, state->pc, interrupt_num * 8);
}

static void unimplemented_instruction(State8080 *state)
{
    unsigned char *opcode = &state->memory[state->pc - 1];
    printf("Error: Unimplemented instruction\n");
    printf("0x%x\n", *opcode);
    exit(1);
}

State8080 *init_8080(void)
{
    State8080 *state = calloc(1, sizeof(State8080));
    state->memory = malloc(0x10000); // 16K
    return state;
}

void emulate_8080_op(State8080 *state)
{
    unsigned char *opcode = &state->memory[state->pc];

    state->pc += 1;

    switch (*opcode)
    {
    case 0x00: // NOP
        break;
    case 0x01: // LXI B, Word
        state->c = opcode[1];
        state->b = opcode[2];
        state->pc += 2;
        break;
    case 0x02: // STAX B
    {
        write_mem(state, get_bc(state), state->a);
        break;
    }
    case 0x03: // INX B
    {
        set_bc(state, get_bc(state) + 1);
        break;
    }
    case 0x04: // INR B
        state->b = inr(state, state->b);
        break;
    case 0x05: // DCR B
        state->b = dcr(state, state->b);
        break;
    case 0x06: // MVI B, D8
        state->b = opcode[1];
        state->pc++;
        break;
    case 0x07: // RLC
    {
        uint8_t bit_seven = state->a >> 7;
        state->a = (state->a << 1) | bit_seven;
        state->cc.cy = bit_seven;
        break;
    }
    case 0x08: // Unused
        unimplemented_instruction(state);
        break;
    case 0x09: // DAD B
    {
        dad(state, get_bc(state));
        break;
    }
    case 0x0a: // LDA B
    {
        state->a = state->memory[get_bc(state)];
        break;
    }
    case 0x0b: // DCX B
    {
        set_bc(state, get_bc(state) - 1);
        break;
    }
    case 0x0c: // INR C
        state->c = inr(state, state->c);
        break;
    case 0x0d: // DCR C
        state->c = dcr(state, state->c);
        break;
    case 0x0e: // MVI C, D8
        state->c = opcode[1];
        state->pc++;
        break;
    case 0x0f: // RRC
    {
        uint8_t bit_zero = state->a & 0x1;
        state->a = (state->a >> 1) | (bit_zero << 7);
        state->cc.cy = bit_zero;
        break;
    }
    case 0x10: // Unused
        unimplemented_instruction(state);
        break;
    case 0x11: // LXI D, Word
        state->e = opcode[1];
        state->d = opcode[2];
        state->pc += 2;
        break;
    case 0x12: // STAX D
        write_mem(state, get_de(state), state->a);
        break;
    case 0x13: // INX D
        set_de(state, get_de(state) + 1);
        break;
    case 0x14: // INR D
        state->d = inr(state, state->d);
        break;
    case 0x15: // DCR D
        state->d = dcr(state, state->d);
        break;
    case 0x16: // MVI D, D8
        state->d = opcode[1];
        state->pc++;
        break;
    case 0x17: // RAL
    {
        uint8_t bit_seven = state->a >> 7;
        state->a = (state->a << 1) | state->cc.cy;
        state->cc.cy = bit_seven;
        break;
    }
    case 0x18: // Unused
        unimplemented_instruction(state);
        break;
    case 0x19: // DAD D
        dad(state, get_de(state));
        break;
    case 0x1a: // LDA D
        state->a = state->memory[get_de(state)];
        break;
    case 0x1b: // DCX D
        set_de(state, get_de(state) - 1);
        break;
    case 0x1c: // INR E
        state->e = inr(state, state->e);
        break;
    case 0x1d: // DCR E
        state->e = dcr(state, state->e);
        break;
    case 0x1e: // MVI E, Byte
        state->e = opcode[1];
        state->pc++;
        break;
    case 0x1f: // RAR
    {
        uint8_t bit_zero = state->a & 0x1;
        state->a = (state->a >> 1) | ((state->cc.cy & 1) << 7);
        state->cc.cy = bit_zero;
        break;
    }
    case 0x20: // Unused
        unimplemented_instruction(state);
        break;
    case 0x21: // LXI H, Word
        state->l = opcode[1];
        state->h = opcode[2];
        state->pc += 2;
        break;
    case 0x22: // ShlD Address
    {
        uint16_t offset = (opcode[2] << 8) | opcode[1];
        write_mem(state, offset, state->l);
        write_mem(state, offset + 1, state->h);
        state->pc += 2;
        break;
    }
    case 0x23: // INX H
        set_hl(state, get_hl(state) + 1);
        break;
    case 0x24: // INR H
        state->h = inr(state, state->h);
        break;
    case 0x25: // DCR H
        state->h = dcr(state, state->h);
        break;
    case 0x26: // MVI H, D8
        state->h = opcode[1];
        state->pc++;
        break;
    case 0x27: // DAA
    {
        bool cy = state->cc.cy;
        uint8_t correction = 0;

        uint8_t lsb = state->a & 0x0F;
        uint8_t msb = state->a >> 4;

        if (state->cc.ac || lsb > 9)
        {
            correction += 0x06;
        }

        if (state->cc.cy || msb > 9 || (msb >= 9 && lsb > 9))
        {
            correction += 0x60;
            cy = 1;
        }

        add(state, &state->a, correction, 0);
        state->cc.cy = cy;
        break;
    }
    case 0x28: // Unused
        unimplemented_instruction(state);
        break;
    case 0x29: // DAD H
        dad(state, get_hl(state));
        break;
    case 0x2a: // LhlD Address
    {
        uint16_t offset = (opcode[2] << 8) | opcode[1];
        state->l = state->memory[offset];
        state->h = state->memory[offset + 1];
        state->pc += 2;
        break;
    }
    case 0x2b: // DCX H
        set_hl(state, get_hl(state) - 1);
        break;
    case 0x2c: // INR L
        state->l = inr(state, state->l);
        break;
    case 0x2d: // DCR L
        state->l = dcr(state, state->l);
        break;
    case 0x2e: // MVI L, D8
        state->l = opcode[1];
        state->pc++;
        break;
    case 0x2f: // CMA
        state->a = ~state->a;
        break;
    case 0x30: // Unused
        unimplemented_instruction(state);
        break;
    case 0x31: // LXI SP, Word
        state->sp = (opcode[2] << 8) | opcode[1];
        state->pc += 2;
        break;
    case 0x32: // STAX Address
    {
        uint16_t offset = (opcode[2] << 8) | opcode[1];
        write_mem(state, offset, state->a);
        state->pc += 2;
        break;
    }
    case 0x33: // INX SP
        state->sp++;
        break;
    case 0x34: // INR M
    {
        uint8_t result = inr(state, read_from_m(state));
        write_to_m(state, result);
        break;
    }
    case 0x35: // DCR M
    {
        uint8_t result = dcr(state, read_from_m(state));
        write_to_m(state, result);
        break;
    }
    case 0x36: // MVI M, D8
        write_to_m(state, opcode[1]);
        state->pc++;
        break;
    case 0x37: // STC
        state->cc.cy = 1;
        break;
    case 0x38: // Unused
        unimplemented_instruction(state);
        break;
    case 0x39: // DAD SP
        dad(state, state->sp);
        break;
    case 0x3a: // LDA Address
    {
        uint16_t offset = (opcode[2] << 8) | opcode[1];
        state->a = state->memory[offset];
        state->pc += 2;
        break;
    }
    case 0x3b: // DCX SP
        state->sp--;
        break;
    case 0x3c: // INR A
        state->a = inr(state, state->a);
        break;
    case 0x3d: // DCR A
        state->a = dcr(state, state->a);
        break;
    case 0x3e: // MVI A,D8
        state->a = opcode[1];
        state->pc++;
        break;
    case 0x3f: // CMC
        state->cc.cy = !state->cc.cy;
        break;
    case 0x40: // MOV B, B
        break;
    case 0x41: // MOV B, C
        state->b = state->c;
        break;
    case 0x42: // MOV B, D
        state->b = state->d;
        break;
    case 0x43: // MOV B, E
        state->b = state->e;
        break;
    case 0x44: // MOV B, H
        state->b = state->h;
        break;
    case 0x45: // MOV B, L
        state->b = state->l;
        break;
    case 0x46: // MOV B, M
        state->b = read_from_m(state);
        break;
    case 0x47: // MOV B, A
        state->b = state->a;
        break;
    case 0x48: // MOV C, B
        state->c = state->b;
        break;
    case 0x49: // MOV, C, C
        break;
    case 0x4a: // MOV C, D
        state->c = state->d;
        break;
    case 0x4b: // MOV C, E
        state->c = state->e;
        break;
    case 0x4c: // MOV C, H
        state->c = state->h;
        break;
    case 0x4d: // MOV C, L
        state->c = state->l;
        break;
    case 0x4e: // MOV C, M
        state->c = read_from_m(state);
        break;
    case 0x4f: // MOV C, A
        state->c = state->a;
        break;
    case 0x50: // MOV D, B
        state->d = state->b;
        break;
    case 0x51: // MOV D, C
        state->d = state->c;
        break;
    case 0x52: // MOV D, D
        break;
    case 0x53: // MOV D, E
        state->d = state->e;
        break;
    case 0x54: // MOV D, H
        state->d = state->h;
        break;
    case 0x55: // MOV D, L
        state->d = state->l;
        break;
    case 0x56: // MOV D, M
        state->d = read_from_m(state);
        break;
    case 0x57: // MOV D, A
        state->d = state->a;
        break;
    case 0x58: // MOV E, B
        state->e = state->b;
        break;
    case 0x59: // MOV E, C
        state->e = state->c;
        break;
    case 0x5a: // MOV E, D
        state->e = state->d;
        break;
    case 0x5b: // MOV E, E
        break;
    case 0x5c: // MOV E, H
        state->e = state->h;
        break;
    case 0x5d: // MOV E, L
        state->e = state->l;
        break;
    case 0x5e: // MOV E, M
        state->e = read_from_m(state);
        break;
    case 0x5f: // MOV E, A
        state->e = state->a;
        break;
    case 0x60: // MOV H, B
        state->h = state->b;
        break;
    case 0x61: // MOV H, C
        state->h = state->c;
        break;
    case 0x62: // MOV H, D
        state->h = state->d;
        break;
    case 0x63: // MOV H, E
        state->h = state->e;
        break;
    case 0x64: // MOV H, H
        break;
    case 0x65: // MOV H, L
        state->h = state->l;
        break;
    case 0x66: // MOV H, M
        state->h = read_from_m(state);
        break;
    case 0x67: // MOV  H, A
        state->h = state->a;
        break;
    case 0x68: // MOV L, B
        state->l = state->b;
        break;
    case 0x69: // MOV L, C
        state->l = state->c;
        break;
    case 0x6a: // MOV L, D
        state->l = state->d;
        break;
    case 0x6b: // MOV L, E
        state->l = state->e;
        break;
    case 0x6c: // MOV L, H
        state->l = state->h;
        break;
    case 0x6d: // MOV L, L
        break;
    case 0x6e: // MOV L, M
        state->l = read_from_m(state);
        break;
    case 0x6f: // MOV L, A
        state->l = state->a;
        break;
    case 0x70: // MOV M, B
        write_to_m(state, state->b);
        break;
    case 0x71: // MOV M, C
        write_to_m(state, state->c);
        break;
    case 0x72: // MOV M, D
        write_to_m(state, state->d);
        break;
    case 0x73: // MOV M, E
        write_to_m(state, state->e);
        break;
    case 0x74: // MOV M, H
        write_to_m(state, state->h);
        break;
    case 0x75: // MOV M, L
        write_to_m(state, state->l);
        break;
    case 0x76: // hlT
        unimplemented_instruction(state);
        break;
    case 0x77: // MOV M, A
        write_to_m(state, state->a);
        break;
    case 0x78: // MOV A, B
        state->a = state->b;
        break;
    case 0x79: // MOV A, C
        state->a = state->c;
        break;
    case 0x7a: // MOV A, D
        state->a = state->d;
        break;
    case 0x7b: // MOV A, E
        state->a = state->e;
        break;
    case 0x7c: // MOV A, H
        state->a = state->h;
        break;
    case 0x7d: // MOV A, L
        state->a = state->l;
        break;
    case 0x7e: // MOV A, M
        state->a = read_from_m(state);
        break;
    case 0x7f: // MOV A, A
        break;
    case 0x80: // ADD B
        add(state, &state->a, state->b, 0);
        break;
    case 0x81: // ADD C
        add(state, &state->a, state->c, 0);
        break;
    case 0x82: // ADD D
        add(state, &state->a, state->d, 0);
        break;
    case 0x83: // ADD E
        add(state, &state->a, state->e, 0);
        break;
    case 0x84: // ADD H
        add(state, &state->a, state->h, 0);
        break;
    case 0x85: // ADD L
        add(state, &state->a, state->l, 0);
        break;
    case 0x86: // ADD M
        add(state, &state->a, read_from_m(state), 0);
        break;
    case 0x87: // ADD A
        add(state, &state->a, state->a, 0);
        break;
    case 0x88: // ADC B
        add(state, &state->a, state->b, state->cc.cy);
        break;
    case 0x89: // ADC C
        add(state, &state->a, state->c, state->cc.cy);
        break;
    case 0x8a: // ADC D
        add(state, &state->a, state->d, state->cc.cy);
        break;
    case 0x8b: // ADC E
        add(state, &state->a, state->e, state->cc.cy);
        break;
    case 0x8c: // ADC H
        add(state, &state->a, state->h, state->cc.cy);
        break;
    case 0x8d: // ADC L
        add(state, &state->a, state->l, state->cc.cy);
        break;
    case 0x8e: // ADC M
        add(state, &state->a, read_from_m(state), state->cc.cy);
        break;
    case 0x8f: // ADC A
        add(state, &state->a, state->a, state->cc.cy);
        break;
    case 0x90: // SUB B
        substract(state, &state->a, state->b, 0);
        break;
    case 0x91: // SUB C
        substract(state, &state->a, state->c, 0);
        break;
    case 0x92: // SUB D
        substract(state, &state->a, state->d, 0);
        break;
    case 0x93: // SUB E
        substract(state, &state->a, state->e, 0);
        break;
    case 0x94: // SUB H
        substract(state, &state->a, state->h, 0);
        break;
    case 0x95: // SUB L
        substract(state, &state->a, state->l, 0);
        break;
    case 0x96: // SUB M
        substract(state, &state->a, read_from_m(state), 0);
        break;
    case 0x97: // SUB A
        substract(state, &state->a, state->a, 0);
        break;
    case 0x98: // SBB B
        substract(state, &state->a, state->b, state->cc.cy);
        break;
    case 0x99: // SBB C
        substract(state, &state->a, state->c, state->cc.cy);
        break;
    case 0x9a: // SBB D
        substract(state, &state->a, state->d, state->cc.cy);
        break;
    case 0x9b: // SBB E
        substract(state, &state->a, state->e, state->cc.cy);
        break;
    case 0x9c: // SBB H
        substract(state, &state->a, state->h, state->cc.cy);
        break;
    case 0x9d: // SBB L
        substract(state, &state->a, state->l, state->cc.cy);
        break;
    case 0x9e: // SBB M
        substract(state, &state->a, read_from_m(state), state->cc.cy);
        break;
    case 0x9f: // SBB A
        substract(state, &state->a, state->a, state->cc.cy);
        break;
    case 0xa0: // ANA B
        ana(state, state->b);
        break;
    case 0xa1: // ANA C
        ana(state, state->c);
        break;
    case 0xa2: // ANA D
        ana(state, state->d);
        break;
    case 0xa3: // ANA E
        ana(state, state->e);
        break;
    case 0xa4: // ANA H
        ana(state, state->h);
        break;
    case 0xa5: // ANA L
        ana(state, state->l);
        break;
    case 0xa6: // ANA M
        ana(state, read_from_m(state));
        break;
    case 0xa7: // ANA A
        ana(state, state->a);
        break;
    case 0xa8: // XRA B
        xra(state, state->b);
        break;
    case 0xa9: // XRA C
        xra(state, state->c);
        break;
    case 0xaa: // XRA D
        xra(state, state->d);
        break;
    case 0xab: // XRA E
        xra(state, state->e);
        break;
    case 0xac: // XRA H
        xra(state, state->h);
        break;
    case 0xad: // XRA L
        xra(state, state->l);
        break;
    case 0xae: // XRA M
        xra(state, read_from_m(state));
        break;
    case 0xaf: // XRA A
        xra(state, state->a);
        break;
    case 0xb0: // ORA B
        ora(state, state->b);
        break;
    case 0xb1: // ORA C
        ora(state, state->c);
        break;
    case 0xb2: // ORA D
        ora(state, state->d);
        break;
    case 0xb3: // ORA E
        ora(state, state->e);
        break;
    case 0xb4: // ORA H
        ora(state, state->h);
        break;
    case 0xb5: // ORA L
        ora(state, state->l);
        break;
    case 0xb6: // ORA M
        ora(state, read_from_m(state));
        break;
    case 0xb7: // ORA A
        ora(state, state->a);
        break;
    case 0xb8: // CMP B
        cmp(state, state->b);
        break;
    case 0xb9: // CMP C
        cmp(state, state->c);
        break;
    case 0xba: // CMP D
        cmp(state, state->d);
        break;
    case 0xbb: // CMP E
        cmp(state, state->e);
        break;
    case 0xbc: // CMP H
        cmp(state, state->h);
        break;
    case 0xbd: // CMP L
        cmp(state, state->l);
        break;
    case 0xbe: // CMP M
        cmp(state, read_from_m(state));
        break;
    case 0xbf: // CMP A
        cmp(state, state->a);
        break;
    case 0xc0: // RNZ
        if (state->cc.z == 0)
        {
            ret(state);
        }
        break;
    case 0xc1: // POP B
        pop(state, &state->b, &state->c);
        break;
    case 0xc2: // JNZ Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_jump(state, address, state->cc.z == 0);
        break;
    }
    case 0xc3: // JMP Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        state->pc = address;
        break;
    }
    case 0xc4: // CNZ Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_call(state, address, state->cc.z == 0);
        break;
    }
    case 0xc5: // push B
        push(state, state->b, state->c);
        break;
    case 0xc6: // ADI Byte
        add(state, &state->a, opcode[1], 0);
        state->pc++;
        break;
    case 0xc7: // RST 0
        call(state, state->pc, 0x0000);
        break;
    case 0xc8: // RZ
        if (state->cc.z)
        {
            ret(state);
        }
        break;
    case 0xc9: // RET
        ret(state);
        break;
    case 0xca: // JZ Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_jump(state, address, state->cc.z);
        break;
    }
    case 0xcb: // Unused
        unimplemented_instruction(state);
        break;
    case 0xcc: // CZ Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_call(state, address, state->cc.z);
        break;
    }
    case 0xcd: // call Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        call(state, state->pc + 2, address);
        break;
    }
    case 0xce: // ACI Byte
        add(state, &state->a, opcode[1], state->cc.cy);
        state->pc++;
        break;
    case 0xcf: // RST 1
        call(state, state->pc, 0x0008);
        break;
    case 0xd0: // RNC
        if (state->cc.cy == 0)
        {
            ret(state);
        }
        break;
    case 0xd1: // POP D
        pop(state, &state->d, &state->e);
        break;
    case 0xd2: // JNC Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_jump(state, address, state->cc.cy == 0);
        break;
    }
    case 0xd3: // OUT Byte
        state->port_output(state->user_data, opcode[1], state->a);
        state->pc++;
        break;
    case 0xd4: // CNC Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_call(state, address, state->cc.cy == 0);
        break;
    }
    case 0xd5: // push D
        push(state, state->d, state->e);
        break;
    case 0xd6: // SUI Byte
        substract(state, &state->a, opcode[1], 0);
        state->pc++;
        break;
    case 0xd7: // RST 2
        call(state, state->pc, 0x0010);
        break;
    case 0xd8: // RC
        if (state->cc.cy)
        {
            ret(state);
        }
        break;
    case 0xd9: // Unused
        unimplemented_instruction(state);
        break;
    case 0xda: // JC Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_jump(state, address, state->cc.cy);
        break;
    }
    case 0xdb: // IN Byte
        state->a = state->port_input(state->user_data, opcode[1]);
        state->pc++;
        break;
    case 0xdc: // CC Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_call(state, address, state->cc.cy);
        break;
    }
    case 0xdd: // Unused
        unimplemented_instruction(state);
        break;
    case 0xde: // SBI Byte
        substract(state, &state->a, opcode[1], state->cc.cy);
        state->pc++;
        break;
    case 0xdf: // RST 3
        call(state, state->pc, 0x0018);
        break;
    case 0xe0: // RPO
        if (state->cc.p == 0)
        {
            ret(state);
        }
        break;
    case 0xe1: // POP H
        pop(state, &state->h, &state->l);
        break;
    case 0xe2: // JPO Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_jump(state, address, state->cc.p == 0);
        break;
    }
    case 0xe3: // XThl
    {
        uint8_t l = state->l;
        uint8_t h = state->h;
        state->l = state->memory[state->sp];
        state->h = state->memory[state->sp + 1];
        write_mem(state, state->sp, l);
        write_mem(state, state->sp + 1, h);
        break;
    }
    case 0xe4: // CPO Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_call(state, address, state->cc.p == 0);
        break;
    }
    case 0xe5: // push H
        push(state, state->h, state->l);
        break;
    case 0xe6: // ANI Byte
        ana(state, opcode[1]);
        state->pc++;
        break;
    case 0xe7: // RST 4
        call(state, state->pc, 0x0020);
        break;
    case 0xe8: // RPE
        if (state->cc.p)
        {
            ret(state);
        }
        break;
    case 0xe9: // PChl
        state->pc = get_hl(state);
        break;
    case 0xea: // JPE Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_jump(state, address, state->cc.p);
        break;
    }
    case 0xeb: // XCHG
    {
        uint16_t de = get_de(state);
        set_de(state, get_hl(state));
        set_hl(state, de);
        break;
    }
    case 0xec: // CPE Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_call(state, address, state->cc.p);
        break;
    }
    case 0xed: // Unused
        unimplemented_instruction(state);
        break;
    case 0xee: // XRI Byte
        xra(state, opcode[1]);
        state->pc++;
        break;
    case 0xef: // RST 5
        call(state, state->pc, 0x0028);
        break;
    case 0xf0: // RP
        if (state->cc.s == 0)
        {
            ret(state);
        }
        break;
    case 0xf1: // POP PSW
    {
        uint8_t a = state->memory[state->sp + 1];
        uint8_t psw = state->memory[state->sp];
        state->sp += 2;
        state->a = a;

        state->cc.s = (psw >> 7) & 1;
        state->cc.z = (psw >> 6) & 1;
        state->cc.ac = (psw >> 4) & 1;
        state->cc.p = (psw >> 2) & 1;
        state->cc.cy = (psw >> 0) & 1;
        break;
    }
    case 0xf2: // JP Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_jump(state, address, state->cc.s == 0);
        break;
    }
    case 0xf3: // DI
        state->int_enable = 0;
        break;
    case 0xf4: // CP Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_call(state, address, state->cc.s == 0);
        break;
    }
    case 0xf5: // push PSW
    {
        uint8_t psw = 0;
        psw |= state->cc.s << 7;
        psw |= state->cc.z << 6;
        psw |= state->cc.ac << 4;
        psw |= state->cc.p << 2;
        psw |= 1 << 1; // bit 1 is always 1
        psw |= state->cc.cy << 0;
        write_mem(state, state->sp - 1, state->a);
        write_mem(state, state->sp - 2, psw);
        state->sp -= 2;
        break;
    }
    case 0xf6: // ORI Byte
        ora(state, opcode[1]);
        state->pc++;
        break;
    case 0xf7: // RST 6
        call(state, state->pc, 0x0030);
        break;
    case 0xf8: // RM
        if (state->cc.s)
        {
            ret(state);
        }
        break;
    case 0xf9: // SPhl
        state->sp = get_hl(state);
        break;
    case 0xfa: // JM Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_jump(state, address, state->cc.s);
        break;
    }
    case 0xfb: // EI
        state->int_enable = 1;
        break;
    case 0xfc: // CM Address
    {
        uint16_t address = (opcode[2] << 8) | opcode[1];
        cond_call(state, address, state->cc.s);
        break;
    }
    case 0xfd: // Unused
        unimplemented_instruction(state);
        break;
    case 0xfe: // CPI Byte
    {
        cmp(state, opcode[1]);
        state->pc++;
        break;
    }
    case 0xff: // RST 7
        call(state, state->pc, 0x0038);
        break;
    }

    state->cycle_count += cycles8080[*opcode];
    return;
}
