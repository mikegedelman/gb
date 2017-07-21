#pragma once

#include <stdint.h>

typedef struct {
	uint8_t *mem;
} audio_t;

audio_t *audio_init(uint8_t*);
void audio_update(audio_t*);