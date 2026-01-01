#pragma once

#include "lfs.h"
#include <stdbool.h>

#define SAMPLES_PER_BUFFER 256
#define AUDIO_MAX_GAIN 32767

extern lfs_t lfs;
extern volatile bool wakeup;

void init_audio();
void play_audio(char* filename, uint16_t gain);
