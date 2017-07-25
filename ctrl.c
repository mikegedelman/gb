#include "ctrl.h"

#include <stdio.h>

#define VBLANK_INT 16666 /* (60 / second) * 1000000 */
#define VBLANK_INC 122 /* 1000 / 9 */
 
#define LY mem[0xFF44]

static uint32_t vb_start, vb_next_inc;
	
void ctrl_init() {
	vb_start = VBLANK_INT;
	vb_next_inc = 0;
}

void interrupts(uint8_t *mem, uint32_t cycles)
{
	if (LY && cycles > vb_next_inc) {
		LY++;
		if (LY > 153) {
			LY = 0;
		} else {
			vb_next_inc += VBLANK_INC;
		}
	} else if (cycles > vb_start) {
		vb_start = cycles + VBLANK_INT;
		vb_next_inc = vb_start + VBLANK_INC;
		LY = 0x90;
	}
}