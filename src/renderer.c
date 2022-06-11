#include <stdbool.h>
#include <stdint.h>
#include <SDL.h>
#include "renderer.h"

SDL_Window *game_window = NULL;
SDL_Renderer *game_renderer = NULL;
SDL_Surface *game_screen_surface = NULL;

static void clear_window()
{
    SDL_RenderClear(game_renderer);
}

static void update_window()
{
    SDL_SetRenderDrawColor(game_renderer, 0, 0, 0, 0);
    SDL_UpdateWindowSurface(game_window);
}

void draw_screen(uint8_t *bitmap_buffer)
{
    clear_window();
    SDL_SetRenderDrawColor(game_renderer, 255, 255, 255, 0);

    for (int x = 0; x < ORIGINAL_WINDOW_WIDTH; x++)
    {
        for (int y = 0; y < ORIGINAL_WINDOW_HEIGHT; y += 8)
        {
            int byte_position = x * (ORIGINAL_WINDOW_HEIGHT / 8) + (ORIGINAL_WINDOW_HEIGHT - 1 - y) / 8;

            for (int j = 0; j < 8; j++)
            {
                if (bitmap_buffer[byte_position] & (1 << (7 - j)))
                {
                    for (int k = 0; k < SCREEN_STRETCH_FACTOR; k++)
                    {
                        SDL_RenderDrawPoint(game_renderer, (x * SCREEN_STRETCH_FACTOR) + k, ((y + j) * SCREEN_STRETCH_FACTOR) + k);
                    }
                }
            }
        }
    }

    update_window();
}

void window_close()
{
    SDL_DestroyWindow(game_window);
    game_window = NULL;
    SDL_Quit();
}

bool window_init()
{
    bool success = true;
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n",
               SDL_GetError());
        success = false;
    }
    else
    {
        game_window = SDL_CreateWindow(
            "Space Invaders", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
        if (game_window == NULL)
        {
            printf("Window could not be created! SDL_Error: %s\n",
                   SDL_GetError());
            success = false;
        }
        else
        {
            game_screen_surface = SDL_GetWindowSurface(game_window);
            game_renderer = SDL_CreateSoftwareRenderer(game_screen_surface);
        }
    }

    return success;
}
