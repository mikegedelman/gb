#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <assert.h>

#include <sys/time.h>

#include "cpu.h"
#include "video.h"
#include "audio.h"
#include "ctrl.h"

#define MAX_ROM_SIZE (1024*1024*1024) /* 1G */

/*
	TODO

	CPU
	- CB table
	- misc instructions

	VIDEO
	- foreground/window video logic
	- OAM, sprite logic

	AUDIO
	- everything

	INPUT
	- everything

*/

uint8_t should_exit = 0;
uint32_t cycles = 0;

typedef struct {
	uint8_t *mem;
	cpu_t *cpu;
	display_t *displ;
	audio_t *audio;
} gb_t;

void load_rom(gb_t *gb, const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		puts("Error opening ROM");
		exit(1);
	}

	size_t len = fread(gb->mem, sizeof(uint8_t), 0x7FFF, fp);
	if (len <= 0) {
		puts("Couldn't read ROM");
		exit(1);
	}
}
void load_cartridge(gb_t *gb, const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		puts("Error opening ROM");
		exit(1);
	}

	int r = fseek(fp, 0x100, SEEK_SET);
	if (r) {
		puts("Couldn't seek to byte 256");
		printf("%d %d\n", errno, r);
		exit(1);
	}

	size_t len = fread(gb->mem + 0x100, sizeof(uint8_t), 0x7FFF - 0x100, fp);
	if (len <= 0) {
		puts("Couldn't read ROM");
		exit(1);
	}
}

gb_t *gameboy_init()
{
	uint8_t *mem = (uint8_t*) calloc(1, 65535);
	cpu_t* cpu = cpu_init(mem);
	gb_t* gb = calloc(1, sizeof(gb_t));
	gb->cpu = cpu;
	gb->mem = mem;
	gb->displ = display_init(mem);
	gb->audio = audio_init(mem);
	ctrl_init();

	return gb;
}
void gameboy_destroy(gb_t *gb) {
	display_destroy(gb->displ);
}

uint8_t update(gb_t *gb)
{
	display_update(gb->displ);
	audio_update(gb->audio);
	interrupts(gb->mem, cycles);
	
	return 1;
}

void* cpu_worker(void *arg)
{
	gb_t *gb = (gb_t*) arg;

	uint8_t breakF = 0;
	while (1) {
		cycles += step(gb->cpu);

		// if (gb->cpu->pc == 0x8c) {
		// 	breakF = 1;
		// }
		// 	// printf("%02x\n", gb->cpu->pc);
		// 	uint8_t *cur_instr = gb->cpu->mem + gb->cpu->pc;
		// 	uint8_t op_code = cur_instr[0];
		// 	uint8_t a1 = cur_instr[1];
		// 	uint8_t a2 = cur_instr[2];

		// 	printf("%02x %02x %02x| ", op_code, a1, a2);
		// 	cpu_print(gb->cpu);
		// 	// printf("\n%02x\n", gb->cpu->mem[0xFF42]);

		// if (breakF) {
		// 	getchar();
		// 	breakF = 0;
		// 	// printf("%02x", gb->cpu->mem[0x0104]);
		// }

		if (should_exit) {
			return NULL;
		}
	}
}


void sigint()
{
	should_exit = 1;
}


int main()
{
	SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
	gb_t *gb = gameboy_init();
	load_rom(gb, "DMG_ROM.bin");
	load_cartridge(gb, "tetris.gb");

	signal(SIGINT, sigint);

	pthread_t cpu_thr;
	int result = pthread_create(&cpu_thr, NULL, cpu_worker, gb);
	assert(!result);

	while(1) {
		if(!update(gb)) {
			gameboy_destroy(gb);
			exit(0);
		}

		SDL_Event e;
		SDL_PollEvent(&e);
		if (e.type == SDL_QUIT) {
			should_exit = 1;
		}

		if (should_exit) {
			display_destroy(gb->displ);
			break;
		}

		if (cycles > 0xFFFFFFFF) {
			puts("Whoops, you need to store cycles in a bigger var");
			exit(0);
		}
	}

	result = pthread_join(cpu_thr, NULL);
	assert(!result);

	return 0;
}

