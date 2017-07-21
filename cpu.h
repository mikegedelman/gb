#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// #define r cpu->reg
#define r8 cpu->r.u8
#define r16 cpu->r.u16
#define m cpu->mem


#define A (cpu->r.u8[0])
#define F (cpu->r.u8[1])
#define B (cpu->r.u8[2])
#define C (cpu->r.u8[3])
#define D (cpu->r.u8[4])
#define E (cpu->r.u8[5])
#define H (cpu->r.u8[6])
#define L (cpu->r.u8[7])

#define AF (cpu->r.u16[0])
#define BC (cpu->r.u16[1])
#define DE (cpu->r.u16[2])
#define HL (cpu->r.u16[3])

#define SP cpu->sp
#define PC cpu->pc

#define z (cpu->r.u8[1] & 0x8)
#define setZ(a) if (a) { F |= 0x8; } else { F &= 0x7; }
#define n (cpu->r.u8[1] & 0x4)
#define setN(a) if (a) { F |= 0x4; } else { F &= 0xb; }
#define h (cpu->r.u8[1] & 0x2)
#define setH(a) if (a) { F |= 0x2; } else { F &= 0xd; }
#define c (cpu->r.u8[1] & 1)
#define setC if (a) { F |= 0x1; } else { F &= 0xe; }
#define zeroFlags cpu->r.u8[1] = 0;

#define addCarry(a,b)  (((a + b) & 0x10000) == 0x10000)
#define subCarry(a,b)  (a - b) < 0
#define mulCarry(a,b)  (((a * b) & 0x10000) == 0x10000)
#define xorCarry(a,b)  (((a ^ b) & 0x10000) == 0x10000)
#define addHalfCarry(a,b) ((((a & 0xf) + (b & 0xf)) & 0x10) == 0x10)
#define subHalfCarry(a,b) ((((a & 0xf) - (b & 0xf)) < 0))
#define mulHalfCarry(a,b) ((((a & 0xf) * (b & 0xf)) & 0x10) == 0x10)

union u816 {
	uint8_t u8[2];
	int8_t i8[2];
	uint16_t u16;
};

union reg_u {
	uint8_t u8[8];
	uint16_t u16[4];
};

typedef struct {
	uint8_t *mem;

	// Registers
	union reg_u r;
	uint16_t sp;
	uint16_t pc;

} cpu_t;

cpu_t *cpu_init(uint8_t *mem) {
	cpu_t *cpu = calloc(1, sizeof(cpu_t));
	cpu->mem = mem;
	cpu->pc = 0;

	r16[0] = 0;
	r16[1] = 0;
	r16[2] = 0;
	r16[3] = 0;

	return cpu;
}


void cpu_print(cpu_t*);

// Execute current instruction
void step(cpu_t *cpu)
{
	uint8_t tmp8;
	uint16_t tmp16;
	union u816 work;

	uint16_t sz; // no of instrutions of this op

	uint8_t *cur_instr = cpu->mem + cpu->pc;
	uint8_t op_code = cur_instr[0];
	union u816 arg;
	arg.u8[0] = cur_instr[1];
	arg.u8[1] = cur_instr[2];


	printf("%02x | ", op_code);
	switch (op_code) {
		case 0x00:
			sz = 1;
			break;
		// LD codes
		case 0x01: // LD BC,d16
			BC = arg.u16;
			sz = 3;
			break;

		case 0x02: // LD (BC), A
			// mem_cp_from_cpu(m, rA, *rB, 1);
			m[BC] = A;
			sz = 1;
			break;

		case 0x21: // LD HL, d16
			HL = arg.u16;
			sz = 3;
			break;

		case 0x32: // LD (HL-),A
			m[HL] = A;
			HL--;
			sz = 1;
			break;

		case 0x31: // LD SP, d16
			SP = arg.u16;
			sz = 3;
			break;

		// Arithmetic
		case 0x03: // INC BC
			BC++;
			sz = 1;
			break;

		case 0x04: // INC B z0h-
			tmp8 = B + 1;
			setZ(addCarry(B, tmp8));
			setN(0);
			setH(addHalfCarry(B, tmp8));
			B = tmp8;
			sz = 1;
			break;

		case 0x23: // INC HL
			HL++;
			sz = 1;
			break;

		case 0xAF: // XOR A
			AF = 0; // also zero flags
			setZ(1);
			sz = 1;
			break;

		case 0xCB:
			// printf("%d", (H & 0x80));
			setZ(!(H & 0x80));
			setH(1);
			sz = 2;
			break;

		// Jumps
		case 0x20: // JR NZ,r8 (signed) NZ == (Z == 0)
			if (!z) {
				sz = 0; 
				cpu->pc = 2 + cpu->pc + arg.i8[0];
			} else {
				puts("done");
				exit(0);
			}
			break;

		default:
			puts("Unknown op code.");
			sz = 0;
	}

	cpu->pc += sz;

	cpu_print(cpu);
}

void cpu_print(cpu_t *cpu) {
	//puts("A  F  B  C  D  E  H  L");
	for (uint8_t i = 0; i < 8; i++) {
		printf("%02x ", cpu->r.u8[i]);
	}
	printf("| ");
	printf("pc: %02x sp: %02x | ", PC, SP);
	printf("z%d n%d h%d c%d\n", z, n, h, c);
}
