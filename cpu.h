#pragma once

#include <stdint.h>

typedef struct {
	uint8_t *mem;

	// Registers
	uint8_t r[8];
	uint16_t sp;
	uint16_t pc;

} cpu_t;

cpu_t *cpu_init(uint8_t*);
void cpu_print(cpu_t*);
uint8_t step(cpu_t *cpu);