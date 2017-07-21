#include "ctrl.h"

#include <stdio.h>

#define VBLANK_INT 16666 /* (60 / second) * 1000000 */
#define VBLANK_DURATION 4560 /* from pandocs */
 
static uint32_t vb_start, vb_end;
	
void ctrl_init() {
	vb_start = VBLANK_INT;
	vb_end = 0;
}

void interrupts(uint8_t *mem, uint32_t cycles)
{
	if (cycles > vb_start) {
		vb_start = cycles + VBLANK_INT;
		vb_end = cycles + VBLANK_DURATION;
		mem[0xFF44] = 0x90;
	} else if (cycles > vb_end) {
		mem[0xFF44] = 0;
	}
}