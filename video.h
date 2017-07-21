#pragma once

#include <SDL2/SDL.h>

typedef struct {
	uint8_t *mem;
	SDL_Renderer *ren;
	SDL_Window *win;
	SDL_Texture *tx;
} display_t;


display_t *display_init(uint8_t *mem);
uint8_t display_update(display_t *d);
void display_destroy(display_t *d);