#pragma once
#define SCREEN_STRETCH_FACTOR 3
#define ORIGINAL_WINDOW_WIDTH 224
#define ORIGINAL_WINDOW_HEIGHT 256
#define WINDOW_WIDTH (ORIGINAL_WINDOW_WIDTH * SCREEN_STRETCH_FACTOR)
#define WINDOW_HEIGHT (ORIGINAL_WINDOW_HEIGHT * SCREEN_STRETCH_FACTOR)

void window_close();
bool window_init();
void draw_screen(uint8_t *bitmap_buffer);
