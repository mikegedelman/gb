#include "video.h"

#include <sys/time.h>

#define bit(var,pos) (((var)>>(pos)) & 1)
#define m d->mem

display_t *display_init(uint8_t *mem)
{
	SDL_Window *win = SDL_CreateWindow("GameBoy", 100, 100, 640, 480,
		SDL_WINDOW_SHOWN);
	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	SDL_Texture *tx = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, 
		SDL_TEXTUREACCESS_STREAMING, 160, 144);

	SDL_RenderPresent(ren);

	display_t *d = calloc(1, sizeof(display_t));
	d->mem = mem;
	d->ren = ren;
	d->win = win;
	d->tx = tx;

	return d;
}


void display_destroy(display_t *d)
{
	SDL_DestroyTexture(d->tx);
	SDL_DestroyRenderer(d->ren);
	SDL_DestroyWindow(d->win);
	SDL_Quit();
}

void display_step(display_t *d)
{
	uint8_t lcd_reg = m[0xFF40];

	if (!(lcd_reg & 0x80)) {
		return;
	}
	SDL_SetRenderDrawColor(d->ren, 255, 255, 255, 255);
	SDL_RenderFillRect(d->ren, NULL);

	/* check for v-blank period before looking at VRAM; if nonzero, bail */
	if (m[0xFF44]) {
		return;
	}

	/* set palette */
	uint8_t palette_data = m[0xFF47];

	uint8_t color_0 = palette_data & 3;
	uint8_t color_1 = (palette_data >> 2) & 3;
	uint8_t color_2 = (palette_data >> 4) & 3;
	uint8_t color_3 = (palette_data >> 6) & 3;

	uint8_t color_table[4] = {0xFF, 0xAA, 0x55, 0}; 

	/* render selected BG map -> pixels */
	uint8_t* tile_map = m + 0x9800;
	uint8_t* bg_map = m + 0x8000;
	uint8_t pixbuf[256][256];


	for (int y = 0; y < 32; y++) {
		for (int x = 0; x < 32; x++) {
			uint8_t tile_num = tile_map[(y * 32) + x];

			uint8_t *tile_data = bg_map + (tile_num * 16);

			for (uint8_t row = 0; row < 8; row++) {
				uint8_t b1 = tile_data[row * 2];
				uint8_t b2 = tile_data[(row * 2) + 1];
				for (uint8_t col = 0; col < 8; col++) {
					uint8_t lo_bit = (b1 >> (7 - col)) & 1;
					uint8_t hi_bit = (b2 >> (7 - col)) & 1;
					uint8_t color_num = lo_bit | (hi_bit << 1);
					pixbuf[(x * 8) + col][(y * 8) + row] = color_num;
				}
			}
		}
	}

	int pitch;
	uint8_t *sdl_pixels;
	if(SDL_LockTexture(d->tx, NULL, (void**)&sdl_pixels, &pitch)) {
		printf("SDL_Lock failed: %s\n", SDL_GetError());
		exit(1);
	}

	uint8_t bgX = m[0xFF43];
	uint8_t bgY = m[0xFF42];

	for (int y = 0; y < 144; y++) {
		for (int x = 0; x < 160; x++) {
			uint8_t px_color_num = pixbuf[(bgX + x) % 256][(bgY + y) % 256];
			uint8_t px_color = color_table[px_color_num];

			uint32_t offset = (y * pitch) + (x * 3);
			sdl_pixels[offset] = px_color;
			sdl_pixels[offset + 1] = px_color;
			sdl_pixels[offset + 2] = px_color;
		}
	}

	SDL_UnlockTexture(d->tx);

	SDL_RenderCopy(d->ren, d->tx, NULL, NULL);
	SDL_RenderPresent(d->ren);
}

uint8_t display_update(display_t *d)
{
	display_step(d);
	SDL_Delay(17); /* shoot for 60 hz */

	return 1;
}
