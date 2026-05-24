#include "lightshow.h"
#include "ws2812.h"
#include "pico/time.h"
#include <stdbool.h>

volatile bool led_light_state = false;

#define LIGHTSHOW_FADE_TIME_DELTA_MS 10
#define LIGHTSHOW_FADE_TIME_DELTA ((1.0f/1000.0f)*LIGHTSHOW_FADE_TIME_DELTA_MS)

repeating_timer_t timer;

static bool lightshow_quartic_fade(repeating_timer_t *rt) {
    lightshow_quartic_fade_state_t *s = (lightshow_quartic_fade_state_t *)rt->user_data;
    s->t += LIGHTSHOW_FADE_TIME_DELTA;

    // Normalize
    float progress = s->t / s->duration;
    if (progress > 1.0f)
        progress = 1.0f;

    float curve = progress * progress;
    curve *= curve;  // t^4

    if (s->reverse)
        curve = 1.0f - curve;

    // Extract RGB for individual fading
    uint8_t r = (s->base_color >> 16) & 0xFF;
    uint8_t g = (s->base_color >> 8) & 0xFF;
    uint8_t b = s->base_color & 0xFF;

    // Apply brightness curve
    r = (uint8_t)(r * curve);
    g = (uint8_t)(g * curve);
    b = (uint8_t)(b * curve);

    uint32_t color = (r << 16) | (g << 8) | b;
    ws2812_put_color(color);

    bool running = s->t < s->duration;

    if (!running && s->start_next_reversed) {
        cancel_repeating_timer(&timer);
        s->reverse = !s->reverse;
        s->t = 0;
        s->start_next_reversed = false;
        add_repeating_timer_ms(-LIGHTSHOW_FADE_TIME_DELTA_MS, lightshow_quartic_fade, s, &timer);
    }

    if (!running) led_light_state = !s->reverse;

    return running;
}

void lightshow_fade_setup(lightshow_quartic_fade_state_t *initial_state){
    cancel_repeating_timer(&timer);
    add_repeating_timer_ms(-LIGHTSHOW_FADE_TIME_DELTA_MS, lightshow_quartic_fade, initial_state, &timer);
}

static bool lightshow_turn_off(){
    ws2812_put_color(0);
    return false;
}

void lightshow_flash_setup(lightshow_flash_state_t *initial_state){
    cancel_repeating_timer(&timer);
    ws2812_put_color(initial_state->base_color);
    add_repeating_timer_ms(-initial_state->duration, lightshow_turn_off, NULL, &timer);
}
