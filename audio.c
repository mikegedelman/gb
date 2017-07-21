#include "audio.h"

#include <stdio.h>
#include <SDL2/SDL.h>

audio_t *audio_init(uint8_t *mem)
{
	audio_t *a = calloc(1, sizeof(audio_t*));
	a->mem = mem;
	return a;
}

void audio_update(audio_t* a)
{
	if (!(a->mem[0xFF26] & 0x80)) {
		return;
	}


	printf("%02x %02x %02x %02x\n", 
		a->mem[0xFF11],
a->mem[0xFF12],
a->mem[0xFF13],
a->mem[0xFF14]
		);
	/* Check FF25 here: O1 and O2 terminals */

	// puts("Sound on");
	uint16_t freq_x = ((a->mem[0xFF14] & 0x7) << 8) & a->mem[0xFF13];


	/* Channel 1 */

}