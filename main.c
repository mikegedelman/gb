#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

#include "cpu.h"

#define MAX_ROM_SIZE (1024*1024*1024) // 1G

 
typedef struct {
	uint8_t *mem;
	cpu_t *cpu;
} gb_t;

void load_rom(gb_t *gb, const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		puts("Error opening ROM");
		exit(1);
	}

	size_t len = fread(gb->mem, sizeof(char), 0x7FFF, fp);
	if (len <= 0) {
		puts("Couldn't read ROM");
		exit(1);
	}
}

gb_t *gameboy_init()
{
	uint8_t *mem = (uint8_t*) calloc(1, 65535);
	cpu_t* cpu = cpu_init(mem);
	gb_t* gb = calloc(1, sizeof(gb));
	gb->cpu = cpu;
	gb->mem = mem;

	return gb;
}

void update(gb_t *gb)
{
	step(gb->cpu);
}


int main()
{
	gb_t *gb = gameboy_init();
	load_rom(gb, "DMG_ROM.bin");

	while(1) {
	// for (int i=0; i<10; i++) {
		update(gb);
	}

	return 0;
}

