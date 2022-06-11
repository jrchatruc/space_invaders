#pragma once

#include <stdbool.h>

typedef struct ConditionCodes
{
    bool s, z, ac, p, cy;
} ConditionCodes;

typedef struct State8080
{
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint16_t sp;
    uint16_t pc;
    uint8_t *memory;
    struct ConditionCodes cc;
    uint8_t int_enable;

    uint32_t cycle_count;
    void *user_data;
    void (*write_byte)(void *, uint16_t, uint8_t);
    uint8_t (*port_input)(void *, uint8_t);
    void (*port_output)(void *, uint8_t, uint8_t);
} State8080;

State8080 *init_8080(void);
void emulate_8080_op(State8080 *state);
void generate_interrupt(State8080 *state, int interrupt_num);
