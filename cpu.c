#include "cpu.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define CLOCK_SPEED 1 /* MHZ */
#define CYCLE_DURATION_MICROSECONDS (1 / CLOCK_SPEED)

#define m cpu->mem

#define A (cpu->r[0])
#define F (cpu->r[1])

#define B (cpu->r[2])
#define C (cpu->r[3])

#define D (cpu->r[4])
#define E (cpu->r[5])

#define H (cpu->r[6])
#define L (cpu->r[7])

#define SP cpu->sp
#define PC cpu->pc

#define z (cpu->r[1] & 0x8)
#define setZ(a) if (a) { F |= 0x8; } else { F &= 0x7; }
#define n (cpu->r[1] & 0x4)
#define setN(a) if (a) { F |= 0x4; } else { F &= 0xb; }
#define h (cpu->r[1] & 0x2)
#define setH(a) if (a) { F |= 0x2; } else { F &= 0xd; }
#define c (cpu->r[1] & 1)
#define setC(a) if (a) { F |= 0x1; } else { F &= 0xe; }

#define zeroFlags F = 0;

#define addCarry(a,b)  (((a + b) & 0x10000) == 0x10000)
#define subCarry(a,b)  (a - b) < 0
#define mulCarry(a,b)  (((a * b) & 0x10000) == 0x10000)
#define xorCarry(a,b)  (((a ^ b) & 0x10000) == 0x10000)
#define addHalfCarry(a,b) ((((a & 0xf) + (b & 0xf)) & 0x10) == 0x10)
#define subHalfCarry(a,b) ((((a & 0xf) - (b & 0xf)) < 0))
#define mulHalfCarry(a,b) ((((a & 0xf) * (b & 0xf)) & 0x10) == 0x10)

#define setBC(lo, hi) C = lo; B = hi;
#define setDE(lo, hi) E = lo; D = hi;
#define setHL(lo, hi) L = lo; H = hi;
 
#define memAt(lo, hi) m[(hi << 8) + lo]
#define memBC memAt(C, B)
#define memDE memAt(E, D)
#define memHL memAt(L, H)

#define inc16(hi, lo) lo++; if (lo == 0) { hi++; };
#define dec16(hi, lo) lo--; if (lo == 0xFF) { hi--; }

#define read16(lo, hi) ((hi << 8) + lo)

union u816 {
	uint8_t u8[2];
	int8_t i8[2];
	uint16_t u16;
};

cpu_t *cpu_init(uint8_t *mem)
{
	cpu_t *cpu = calloc(1, sizeof(cpu_t));
	cpu->mem = mem;
	cpu->pc = 0;

	cpu->r[0] = 0;
	cpu->r[1] = 0;
	cpu->r[2] = 0;
	cpu->r[3] = 0;
	cpu->r[4] = 0;
	cpu->r[5] = 0;
	cpu->r[6] = 0;
	cpu->r[7] = 0;
	
	return cpu;
}

void rl(cpu_t *cpu, uint8_t *x)
{
	uint8_t tmp = c & 1;
	setC(x[0] & 0x80);
	x[0] = x[0] << 1;
	x[0] |= tmp; /* bring carry bit back in */
	setZ(x[0] == 0);
	setH(0);
	setN(0);
}

void rlc(cpu_t *cpu, uint8_t *x)
{
	setC(x[0] & 0x80);
	x[0] = x[0] << 1;
	x[0] |= c;
	setH(0);
	setN(0);
	setZ(x[0] == 0);
}

#define and(x) _or(cpu, x)
void _and(cpu_t *cpu, uint8_t x)
{
	A &= x;
	setZ(A == 0);
	setC(0);
	setN(0);
}

#define or(x) _or(cpu, x)
void _or(cpu_t *cpu, uint8_t x)
{
	A |= x;
	setZ(A == 0);
	setC(0);
	setN(0);
	setH(0);
}

#define xor(x) _xor(cpu, x)
void _xor(cpu_t *cpu, uint8_t x)
{
	A ^= x;
	setZ(A == 0);
	setC(0);
	setN(0);
	setH(0);
}

void bit(cpu_t *cpu, uint8_t bit_num, uint8_t *x)
{
	setZ(!(x[0] & (1 << bit_num)));
	setH(1);
}

#define inc(a) _inc(cpu, a)
void _inc(cpu_t *cpu, uint8_t *x) /* z0h- */
{
	uint8_t tmp = x[0] + 1;
	setZ(tmp == 0);
	setN(0);
	setH(addHalfCarry(B, tmp));
	x[0] = tmp;
}

#define dec(a) _dec(cpu, a)
void _dec(cpu_t *cpu, uint8_t *x) /* z1h- */
{
	uint8_t tmp = x[0] - 1;
	setZ(tmp == 0);
	setN(1);
	setH(addHalfCarry(B, tmp));
	x[0] = tmp;
}

#define adc(x) _add(cpu, x + c)
#define add(x) _add(cpu, x)
void _add(cpu_t *cpu, uint8_t x)
{
	setH(addHalfCarry(A, x)); /* TODO */
	setC(A + x > 0xFF); /* TODO */
	A = A + x;
	setZ(A == 0);
	setN(0);
}

#define sbc(x) _sub(cpu, x + c)
#define sub(x) _sub(cpu, x)
void _sub(cpu_t *cpu, uint8_t x)
{
	setH(subHalfCarry(A, x)); /* TODO */
	setC(A > x); /* TODO */
	A = A - x;
	setZ(A == 0);
	setN(1);
}

void cb(cpu_t *cpu, uint8_t opcode)
{
	switch (opcode) {
		case 0x11:
			rl(cpu, &C);
			break;
		case 0x7C:
			bit(cpu, 7, &H);
			break;
		default:
			printf("CB %02x\n", opcode);
			exit(0);
	}
}

#define cp(a) _cp(cpu, a)
void _cp(cpu_t *cpu, uint8_t val)
{
	uint8_t res = A - val;
	setZ(res == 0);
	setN(1);
	setH(subHalfCarry(A, res));
	setC(res > 128);
}

uint8_t step(cpu_t *cpu)
{
	uint8_t cycles = 4;
	uint8_t tmp8;
	uint16_t tmp16;
	union u816 work;

	uint16_t sz = 1; /* no of instructions of this op */

	uint8_t *cur_instr = cpu->mem + cpu->pc;
	uint8_t op_code = cur_instr[0];
	uint8_t a1 = cur_instr[1];
	uint8_t a2 = cur_instr[2];

	switch (op_code) {
		case 0x00:
			sz = 1;
			break;

		/* 8-bit loads */
		case 0x40: /* LD B,B */
			sz = 1;
			cycles = 4;
			break;

		case 0x41: /* LD B, C */
			B = C;
			sz = 1;
			cycles = 4;
			break;

		case 0x42: /* LD B, D */
			B = D;
			sz = 1;
			cycles = 4;
			break;

		case 0x43: /* LD B, E */
			B = E;
			sz = 1;
			cycles = 4;
			break;

		case 0x44: /* LD B, H */
			B = H;
			sz = 1;
			cycles = 4;
			break;

		case 0x45: /* LD B, L */
			B = L;
			sz = 1;
			cycles = 4;
			break;

		case 0x46: /* LD B, (HL) */
			B = memHL;
			sz = 1;
			cycles = 8;
			break;

		case 0x47: /* LD B, A */
			A = B;
			sz = 1;
			cycles = 4;
			break;

		case 0x48: /* LD C, B */
			C = B;
			sz = 1;
			cycles = 4;
			break;

		case 0x49: /* LD C, C */
			sz = 1;
			cycles = 4;
			break;

		case 0x4A: /* LD C, D */
			C = D;
			sz = 1;
			cycles = 4;
			break;

		case 0x4B: /* LD C, E */
			C = E;
			sz = 1;
			cycles = 4;
			break;

		case 0x4C: /* LD C, H */
			C = H;
			sz = 1;
			cycles = 4;
			break;

		case 0x4D: /* LD C, L */
			C = L;
			sz = 1;
			cycles = 4;
			break;

		case 0x4E: /* LD C, (HL) */
			C = memHL;
			sz = 1;
			cycles = 8;
			break;

		case 0x4F: /* LD C, A */
			C = A;
			sz = 1;
			cycles = 4;
			break;

		case 0x50: /* LD D, B */
			D = B;
			sz = 1;
			cycles = 4;
			break;

		case 0x51: /* LD D, C */
			D = C;
			sz = 1;
			cycles = 4;
			break;

		case 0x52: /* LD D, D */
			sz = 1;
			cycles = 4;
			break;

		case 0x53: /* LD D, E */
			D = E;
			sz = 1;
			cycles = 4;
			break;

		case 0x54: /* LD D, H */
			D = H;
			sz = 1;
			cycles = 4;
			break;

		case 0x55: /* LD D, L */
			D = L;
			sz = 1;
			cycles = 4;
			break;

		case 0x56: /* LD D, (HL) */
			D = memHL;
			sz = 1;
			cycles = 8;
			break;

		case 0x57: /* LD D, A */
			D = A;
			sz = 1;
			cycles = 4;
			break;

		case 0x58: /* LD E, B */
			E = B;
			sz = 1;
			cycles = 4;
			break;

		case 0x59: /* LD E, C */
			E = C;
			sz = 1;
			cycles = 4;
			break;

		case 0x5A: /* LD E, D */
			E = D;
			sz = 1;
			cycles = 4;
			break;

		case 0x5B: /* LD E, E */
			sz = 1;
			cycles = 4;
			break;

		case 0x5C: /* LD E, H */
			E = H;
			sz = 1;
			cycles = 4;
			break;

		case 0x5D: /* LD E, L */
			E = L;
			sz = 1;
			cycles = 4;
			break;

		case 0x5E: /* LD E, (HL) */
			E = memHL;
			sz = 1;
			cycles = 8;
			break;

		case 0x5F: /* LD E, A */
			E = A;
			sz = 1;
			cycles = 4;
			break;

		case 0x60: /* LD H, B */
			H = B;
			sz = 1;
			cycles = 4;
			break;

		case 0x61: /* LD H, C */
			H = C;
			sz = 1;
			cycles = 4;
			break;

		case 0x62: /* LD H, D */
			H = D;
			sz = 1;
			cycles = 4;
			break;

		case 0x63: /* LD H, E */
			H = E;
			sz = 1;
			cycles = 4;
			break;

		case 0x64: /* LD H, H */
			sz = 1;
			cycles = 4;
			break;

		case 0x65: /* H, L */
			H = L;
			sz = 1;
			cycles = 4;
			break;

		case 0x66: /* H, (HL) */
			H = memHL;
			sz = 1;
			cycles = 8;
			break;

		case 0x67: /* LD H, A */
			H = A;
			sz = 1;
			cycles = 4;
			break;

		case 0x68: /* LD L, B */
			L = B;
			sz = 1;
			cycles = 4;
			break;

		case 0x69: /* LD L, C */
			L = C;
			sz = 1;
			cycles = 4;
			break;

		case 0x6A: /* LD L, D */
			L = D;
			sz = 1;
			cycles = 4;
			break;

		case 0x6B: /* LD L, E */
			L = E;
			sz = 1;
			cycles = 4;
			break;

		case 0x6C: /* LD L, H */
			L = H;
			sz = 1;
			cycles = 4;
			break;

		case 0x6D: /* LD L, L */
			sz = 1;
			cycles = 4;
			break;

		case 0x6E: /* LD L, (HL) */
			L = memHL;
			sz = 1;
			cycles = 8;
			break;

		case 0x70: /* LD (HL), B */
			memHL = B;
			sz = 1;
			cycles = 8;
			break;

		case 0x71: /* LD (HL), C */
			memHL = C;
			sz = 1;
			cycles = 8;
			break;

		case 0x72: /* LD (HL), D */
			memHL = D;
			sz = 1;
			cycles = 8;
			break;

		case 0x73: /* LD (HL), E */
			memHL = E;
			sz = 1;
			cycles = 8;
			break;

		case 0x74: /* LD (HL), H */
			memHL = H;
			sz = 1;
			cycles = 8;
			break;

		case 0x75: /* LD (HL), L */
			memHL = L;
			sz = 1;
			cycles = 8;
			break;

		case 0x77: /* LD (HL), A */
			memHL = A;
			sz = 1;
			cycles = 8;
			break;

		case 0x78: /* LD A, B */
			A = B;
			sz = 1;
			cycles = 4;
			break;

		case 0x79: /* LD A, C */
			A = C;
			sz = 1;
			cycles = 4;
			break;

		case 0x7A: /* LD A, D */
			A = D;
			sz = 1;
			cycles = 4;
			break;

		case 0x7B: /* LD A, E */
			A = E;
			sz = 1;
			cycles = 4;
			break;

		case 0x7C: /* LD A, H */
			A = H;
			sz = 1;
			cycles = 4;
			break;

		case 0x7D: /* LD A, L */
			A = L;
			sz = 1;
			cycles = 4;
			break;

		case 0x7E: /* LD A, (HL) */
			A = memHL;
			sz = 1;
			cycles = 8;
			break;

		case 0x7F: /* LD A, A */
			sz = 1;
			cycles = 4;
			break;

		/* Other loads */
		case 0x01: /* LD BC,d16  a1 fe a2 ff | B C  = ff fe */
			setBC(a1, a2);
			sz = 3;
			cycles = 12;
			break;

		case 0x11: /* LD DE, d16 */
			setDE(a1, a2);
			sz = 3;
			cycles = 12;
			break;

		case 0x21: /* LD HL, d16 */
			setHL(a1, a2);
			sz = 3;
			cycles = 12;
			break;

		case 0x31: // LD SP, d16
			work.u8[0] = a1;
			work.u8[1] = a2;
			SP = work.u16; // TODO
			sz = 3;
			break;

		case 0x02: /* LD (BC), A */
			memBC = A;
			sz = 1;
			break;

		case 0x12: /* LD (DE), A */
			memDE = A;
			cycles = 8;
			break;

		case 0x22: /* LD (HL+),A */
			memHL = A;
			inc16(H, L);
			sz = 1;
			cycles = 8;
			break;

		case 0x32: /* LD (HL-),A */
			memHL = A;
			dec16(H, L);
			sz = 1;
			cycles = 8;
			break;

		case 0x06: // LD B,d8
			B = a1;
			sz = 2;
			cycles = 8;
			break;

		case 0x16: /* LD D, d8 */
			D = a1;
			sz = 2;
			cycles = 8;
			break;

		case 0x26: /* LD H, d8 */
			H = a1;
			sz = 2;
			cycles = 8;
			break;

		case 0x36: /* LD (HL), d8 */
			memHL = a1;
			sz = 2;
			cycles = 8;
			break;

		case 0x0E: /* LD C, d8 */
			C = a1;
			sz = 2;
			cycles = 8;
			break;

		case 0x1E: /* LD E, d8 */
			E = a1;
			sz = 2;
			cycles = 8;
			break;

		case 0x2E: /* LD L, d8 */
			L = a1;
			sz = 2;
			cycles = 8;
			break;

		case 0x3E: /* LD A, d8 */
			A = a1;
			sz = 2;
			cycles = 8;
			break;

		case 0x0A: /* LD A, (BC) */
			A = memBC;
			cycles = 8;
			break;

		case 0x1A: /* LD A, (DE)  */
			A = memDE;
			sz = 1;
			cycles = 8;
			break;

		case 0x2A: /* LD A, (HL+) */
			A = memHL;
			inc16(H, L);
			cycles = 8;
			break;

		case 0x3A: /* LD A, (HL-) */
			A = memHL;
			dec16(H, L);
			cycles = 8;
			break;

		case 0xE0: /* LDH (a8),A */
			m[0xFF00 + a1] = A;
			sz = 2;
			cycles = 12;
			break;

		case 0xF0: /* LDH A, (a8) */
			A = m[0xFF00 + a1];
			sz = 2;
			cycles = 12;
			break;

		case 0xE2: /* LD (C), A  */
			m[0xFF00 + C] = A;
			sz = 1;
			cycles = 8;
			break;

		case 0xF2: /* LD A, (C) */
			A = m[0xFF00 + C];
			sz = 2;
			cycles = 8;
			break;

		case 0xEA: /* LD (a16), A */
			work.u8[0] = a1;
			work.u8[1] = a2;
			m[work.u16] = A;
			sz = 3;
			cycles = 16;
			break;

		case 0xFA: /* LD A, (a16) */
			work.u8[0] = a1;
			work.u8[1] = a2;
			A = m[work.u16];
			sz = 3;
			cycles = 16;
			break;

		/* Arithmetic */

		case 0x03: // INC BC
			inc16(B, C);
			sz = 1;
			break;

		case 0x13: // INC DE
			inc16(D, E);
			sz = 1;
			cycles = 8;
			break;

		case 0x23: // INC HL
			inc16(H, L);
			sz = 1;
			cycles = 8;
			break;

		case 0x33: /* INC SP */
			SP++;
			sz = 1;
			cycles = 8;
			break;

		case 0x0B: /* DEC BC */
			dec16(B, C);
			sz = 1;
			cycles = 8;
			break;

		case 0x1B: /* DEC DE */
			dec16(D, E);
			sz = 1;
			cycles = 8;
			break;

		case 0x2B: /* DEC HL */
			dec16(H, L);
			sz = 1;
			cycles = 8;
			break;

		case 0x3B: /* DEC SP */
			SP--;
			sz = 1;
			cycles = 8;
			break;

		case 0x04: /* INC B */
			inc(&B);
			sz = 1;
			cycles = 4;
			break;

		case 0x14: /* INC D */
			inc(&D);
			sz = 1;
			cycles = 4;
			break;

		case 0x24: /* INC H */
			inc(&H);
			sz = 1;
			cycles = 4;
			break;

		case 0x34: /* INC (HL) */
			tmp8 = memHL;
			tmp8++;
			memHL = tmp8;
			setZ(tmp8 == 0);
			setN(0);
			setH(addHalfCarry(memHL, tmp8));
			sz = 1;
			cycles = 12;
			break;

		case 0x0C: /* INC C */
			inc(&C);
			cycles = 4;
			sz = 1;
			break;

		case 0x1C: /* INC E */
			inc(&E);
			cycles = 4;
			sz = 1;
			break;

		case 0x2C: /* INC L */
			inc(&L);
			cycles = 4;
			sz = 1;
			break;

		case 0x3C: /* INC A */
			inc(&A);
			cycles = 4;
			sz = 1;
			break;

		case 0x05: /* DEC B */
			dec(&B);
			sz = 1;
			cycles = 4;
			break;

		case 0x15: /* DEC D */
			dec(&D);
			cycles = 4;
			sz = 1;
			break;

		case 0x25: /* DEC H */
			dec(&H);
			cycles = 4;
			sz = 1;
			break;

		case 0x35: /* DEC (HL) */
			tmp8 = memHL;
			tmp8--;
			memHL = tmp8;
			setZ(tmp8 == 0);
			setN(1);
			setH(addHalfCarry(memHL, tmp8));
			sz = 1;
			cycles = 12;
			break;

		case 0x0D: /* DEC C */
			dec(&C);
			sz = 1;
			cycles = 4;
			break;

		case 0x1D: /* DEC E */
			dec(&E);
			sz = 1;
			cycles = 4;
			break;

		case 0x2D: /* DEC L */
			dec(&L);
			sz = 1;
			cycles = 4;
			break;

		case 0x3D: /* DEC A */
			dec(&A);
			sz = 1;
			cycles = 4;
			break;

		case 0x80: /* ADD A, B */
			add(B);
			sz = 1;
			cycles = 4;
			break;

		case 0x81: /* ADD A, C */
			add(C);
			sz = 1;
			cycles = 4;
			break;

		case 0x82: /* ADD A, D */
			add(D);
			sz = 1;
			cycles = 4;
			break;

		case 0x83: /* ADD A, E */
			add(E);
			sz = 1;
			cycles = 4;
			break;

		case 0x84: /* ADD A, H */
			add(H);
			sz = 1;
			cycles = 4;
			break;

		case 0x85: /* ADD A, L */
			add(L);
			sz = 1;
			cycles = 4;
			break;

		case 0x86: // ADD A, (HL)
			tmp8 = memHL;
			setH(addHalfCarry(A, tmp8));
			setC(addCarry(A, tmp8));
			A = A + tmp8;
			setZ(A == 0);
			setN(1);

			sz = 1;
			cycles = 4;
			break;

		case 0x87: /* ADD A, A */
			add(A);
			sz = 1;
			cycles = 4;
			break;

		case 0x88: /* ADC A, B */
			adc(B);
			sz = 1;
			cycles = 4;
			break;

		case 0x89: /* ADC A, C */
			adc(C);
			sz = 1;
			cycles = 4;
			break;

		case 0x8A: /* ADC A, D */
			adc(D);
			sz = 1;
			cycles = 4;
			break;

		case 0x8B: /* ADC A, E */
			adc(E);
			sz = 1;
			cycles = 4;
			break;

		case 0x8C: /* ADC A, H */
			adc(H);
			sz = 1;
			cycles = 4;
			break;

		case 0x8D: /* ADC A, L */
			adc(L);
			sz = 1;
			cycles = 4;
			break;

		case 0x8E: /* ADC A, (HL) */
			adc(memHL);
			sz = 1;
			cycles = 8;
			break;

		case 0x8F: /* ADC A, A */
			adc(A);
			sz = 1;
			cycles = 4;
			break;

		case 0x90: /* SUB B */
			sub(B);
			sz = 1;
			cycles = 4;
			break;

		case 0x91: /* SUB C */
			sub(C);
			break;

		case 0x92: /* SUB D */
			sub(D);
			break;

		case 0x93: /* SUB E */
			sub(E);
			break;

		case 0x94: /* SUB H */
			sub(H);
			break;

		case 0x95: /* SUB L */
			sub(L);
			break;

		case 0x96: /* SUB (HL) */
			sub(memHL);
			cycles = 8;
			break;

		case 0x97: /* SUB A */
			sub(A);
			break;

		case 0x98: /* SBC A, B */
			sbc(B);
			break;

		case 0x99: /* SBC A,C */
			sbc(C);
			sz = 1;
			cycles = 4;
			break;

		case 0x9A: /* SBC A, D */
			sbc(D);
			break;

		case 0x9B: /* SBC A, E */
			sbc(E);
			break;

		case 0x9C: /* SBC A, H */
			sbc(H);
			break;

		case 0x9D: /* SBC A, L */
			sbc(L);
			break;

		case 0x9E: /* SBC A, (HL) */
			sbc(memHL);
			cycles = 8;
			break;

		case 0x9F: /* SBC A, A */
			sbc(A);
			break;

		case 0xA0: /* AND B */
			and(B);
			break;

		case 0xA1: /* AND C */
			and(C);
			break;

		case 0xA2: /* AND D */
			and(D);
			break;

		case 0xA3: /* AND E */
			and(E);
			break;

		case 0xA4: /* AND H */
			and(H);
			break;

		case 0xA5: /* AND L */
			and(L);
			break;

		case 0xA6: /* AND (HL) */
			and(memHL);
			cycles = 8;
			break;

		case 0xA7: /* AND A */
			and(A);
			break;

		case 0xA8: /* XOR B */
			xor(B);
			break;

		case 0xA9: /* XOR C */
			xor(C);
			break;

		case 0xAA: /* XOR D */
			xor(D);
			break;

		case 0xAB: /* XOR E */
			xor(E);
			break;

		case 0xAC: /* XOR H */
			xor(H);
			break;

		case 0xAD: /* XOR L */
			xor(L);
			break;

		case 0xAE: /* XOR (HL) */
			xor(memHL);
			break;

		case 0xAF: /* XOR A */
			xor(A);
			break;

		case 0xB0: /* OR B */
			or(B);
			break;

		case 0xB1: /* OR C */
			or(C);
			break;

		case 0xB2: /* OR D */
			or(D);
			break;

		case 0xB3: /* OR E */
			or(E);
			break;

		case 0xB4: /* OR H */
			or(H);
			break;

		case 0xB5: /* OR L */
			or(L);
			break;

		case 0xB6: /* OR (HL) */
			or(memHL);
			break;

		case 0xB7: /* OR A */
			or(A);
			break;

		case 0xB8: /* CP B */
			cp(B);
			break;

		case 0xB9: /* CP C */
			cp(C);
			break;

		case 0xBA: /* CP D */
			cp(D);
			break;

		case 0xBB: /* CP E */
			cp(E);
			break;

		case 0xBC: /* CP H */
			cp(H);
			break;

		case 0xBD: /* CP L */
			cp(L);
			break;

		case 0xBE: /* CP (HL) */
			cp(memHL);
			break;

		case 0xBF: /* CP A */
			cp(A);
			break;

		case 0xFE: /* CP d8 */
			cp(a1);
			sz = 2;
			cycles = 8;
			break;

		case 0xCB: // prefix TODO
			cb(cpu, a1); 
			sz = 2;
			cycles = 8;
			break;

		case 0x17: /* RLA */
			tmp8 = c & 1;
			setC(A & 0x80);
			A = A << 1;
			A |= tmp8;
			setZ(0);
			setN(0);
			setH(0);
			sz = 1;
			break;

		case 0x2F: /* CPL: complement of A */
			A ^= 0xFF;
			setN(1);
			setH(1);
			break;

		/* Jumps and calls */
		case 0x20: // JR NZ,r8 (signed) NZ == (Z == 0) (if Z bit not set)
			if (!z) {
				work.u8[0] = a1;
				sz = 0; 
				cpu->pc = 2 + cpu->pc + work.i8[0];
				cycles = 12;
			} else {
				sz = 2;
				cycles = 8;
			}
			break;

		case 0x30: /* JR NC, r8 */
			if (!c) {
				work.u8[0] = a1;
				sz = 0; 
				cpu->pc = 2 + cpu->pc + work.i8[0];
				cycles = 12;
			} else {
				sz = 2;
				cycles = 8;
			}
			break;

		case 0x18: // JR r8
			work.u8[0] = a1;
			sz = 0;
			cpu->pc = 2 + cpu->pc + work.i8[0];
			cycles = 12;
			break;


		case 0x28: /* JR Z,r8 (signed) NZ == (Z == 0) (if Z bit set) */
			if (z) {
				work.u8[0] = a1;
				sz = 0; 
				cpu->pc = 2 + cpu->pc + work.i8[0];
			} else {
				sz = 2;
			}
			break;

		case 0x38: /* JR C, r8 */
			if (c) {
				work.u8[0] = a1;
				sz = 0; 
				cpu->pc = 2 + cpu->pc + work.i8[0];
			} else {
				sz = 2;
			}
			break;

		case 0xC0: /* RET NZ PC=(SP), SP=SP+2 */
			if (!z) {
				PC = m[SP];
				SP += 2;
				cycles = 20;
			} else {
				cycles = 8;
			}
			sz = 1;
			break;

		case 0xD0: /* RET NC */
			if (!c) {
				PC = m[SP];
				SP += 2;
				cycles = 20;
			} else {
				cycles = 8;
			}
			sz = 1;
			break;


		case 0xC2: /* JP NZ, a16 */
			if (!z) {
				work.u8[0] = a1;
				work.u8[1] = a2;
				cpu->pc = work.u16;
				sz = 0;
				cycles = 16;
			} else {
				cycles = 12;
				sz = 3;
			}
			break;

		case 0xC3: /* JP NC, a16 */
			if (!c) {
				work.u8[0] = a1;
				work.u8[1] = a2;
				cpu->pc = work.u16;
				sz = 0;
				cycles = 16;
			} else {
				cycles = 12;
				sz = 3;
			}
			break;


		case 0xCD: // call a16: push SP: SP=SP-2, (SP)=PC, PC=nn
			PC += 3;
			work.u16 = PC;
			// SP -= 2;
			SP--;
			m[SP] = work.u8[0];
			SP--;
			m[SP] = work.u8[1];

			PC = read16(a1, a2);
			// sz = 3;
			sz = 0;
			cycles = 24;
			break;

		case 0xC9: // RET
			work.u8[1] = m[SP];
			SP++;
			work.u8[0] = m[SP];
			SP++;

			PC = work.u16;

			cycles = 16;
			sz = 0;
			break;

		// Stack stuff
		case 0xC5: // push BC
			SP--;
			m[SP] = C;
			SP--;
			m[SP] = B;

			sz = 1;
			cycles = 16;
			break;

		case 0xC1: // pop BC
			B = m[SP];
			SP++;
			C = m[SP];
			SP++;

			sz = 1;
			cycles = 12;
			break;

		// Misc
		case 0x76: /* HALT */
			printf("Halt\n");
			exit(1);

		case 0xF3: /* DI TODO */
			break;
		case 0xFB: /* EI TODO */
			break;

		default:
			printf("Unknown op code: %02x\n", op_code);
			// exit(1);
			sz = 0;
	}

	cpu->pc += sz;
	int microseconds = cycles * CYCLE_DURATION_MICROSECONDS;
	usleep(microseconds);

	return cycles;
}

void cpu_print(cpu_t *cpu)
{
	/* puts("A  F  B  C  D  E  H  L"); */
	for (uint8_t i = 0; i < 8; i++) {
		printf("%02x ", cpu->r[i]);
	}
	printf("| ");
	printf("pc: %02x sp: %02x (%02x)| ", PC, SP, m[SP]);
	printf("z%d n%d h%d c%d\n", z, n, h, c);
}