#pragma once
#include <stdbool.h>
#include <stdint.h>

#define LIGHTSHOW_MAX_WHITE_COMPONENT_BRIGHTNESS 0x88

typedef struct {
    float t; // In seconds
    float duration; // In seconds
    bool reverse;
    uint32_t base_color;  // 24 bit RGB
    bool start_next_reversed;
} lightshow_quartic_fade_state_t;

typedef struct {
    uint32_t duration; // In milliseconds
    uint32_t base_color;  // 24 bit RGB
} lightshow_flash_state_t;

void lightshow_fade_setup(lightshow_quartic_fade_state_t *initial_state);
void lightshow_flash_setup(lightshow_flash_state_t *initial_state);
