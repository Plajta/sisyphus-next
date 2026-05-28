#include "lfs.h"
#include <hardware/gpio.h>
#include <pico.h>
#include <math.h>
#include <pico/error.h>
#include <pico/time.h>
#include <stdio.h>
#include <ctype.h>
#include <error.h>
#include <limits.h>
#include "color.h"

#ifdef SISYFOSS_COLOR_VEML3328
#include "sensors/veml3328/veml3328.h"
extern i2c_inst_t *sisyfoss_i2c_inst;
#endif

extern lfs_t lfs;
struct color_match_array color_lut;

/*
 * Accepts normalized RGB values
 *
 * Based on standard RGB to HSV conversion formula (didn't think it's that simple)
 *
 * Inspired by: https://www.geeksforgeeks.org/dsa/program-change-rgb-color-model-hsv-color-model/
 */
void color_rgb_to_hsv(float r, float g, float b, color_measurement *output_hsv){
    float color_max = fmax(r, fmax(g, b));
    float color_min = fmin(r, fmin(g, b));
    float diff = color_max - color_min;

    // if equal it's evaluated as grayscale
    if (color_max == color_min){
        output_hsv->hue = 0;
    }

    // otherwise get hue based on dominant color
    else if (color_max == r)
        output_hsv->hue = fmod(60 * ((g - b) / diff) + 360, 360);
    else if (color_max == g)
        output_hsv->hue = fmod(60 * ((b - r) / diff) + 120, 360);
    else if (color_max == b)
        output_hsv->hue = fmod(60 * ((r - g) / diff) + 240, 360);

    // Black (also so it doesn't explode on 0 division)
    if (color_max == 0)
        output_hsv->saturation = 0;
    else
        output_hsv->saturation = (diff / color_max) * 100;

    // just brightness
    output_hsv->value = color_max * 100;
}

static bool parse_int(char **p, long min, long max, long *out, int base) {
    char *endptr;
    long val = strtol(*p, &endptr, base);
    if (endptr == *p || val < min || val > max) {
        return false;
    }
    *out = val;
    *p = endptr;
    return true;
}

int color_lut_load(){
    lfs_file_t lut_file;
    int8_t error;

    error = lfs_file_open(&lfs, &lut_file, "color_lookup_table", LFS_O_RDONLY);
    if (error != LFS_ERR_OK) return PICO_ERROR_NOT_FOUND;

    struct color_entry *color_entries = NULL;
    size_t color_count = 0;
    size_t color_capacity = 0;

    free(color_lut.data); // Just to be sure, there might have been something
    color_lut.data = NULL;
    color_lut.len = 0;

    char c;
    char buffer[MAX_LUT_LINE_SIZE];
    size_t i = 0;
    int ret;

    while (true) {
        ret = lfs_file_read(&lfs, &lut_file, &c, 1);

        // Break the loop if we're at the end of the file or there is an error
        if (ret < 1) {
            break;
        }

        // This line is too long
        if (i >= MAX_LUT_LINE_SIZE){
            i = 0;
            continue;
        }

        if (c != '\n') {
            buffer[i++] = c;
        }
        else {
            buffer[i] = '\0'; // End it with a NUL just to be sure
            char *p = buffer;
            char *endptr;

            // Get color values
            long hue_ul, sat_ul, val_ul;
            if (!parse_int(&p, 0, 360, &hue_ul, 10) ||
                !parse_int(&p, 0, 100, &sat_ul, 10) ||
                !parse_int(&p, 0, 100, &val_ul, 10)) {
                i = 0;
                continue; // Bad line
            }

            while ((p - buffer) < i && isspace((unsigned char)*p)) p++; // Skip whitespace, with safety

            // Get color code name
            char color_name = *p++;

            if (!isalpha((unsigned char)color_name)) {
                // Bad color code
                i = 0;
                continue;
            }

            while ((p - buffer) < i && isspace((unsigned char)*p)) p++; // Skip whitespace, with safety

            // Get color representation for LED
            long color_representation;
            if(!parse_int(&p, 0, 0xFFFFFF, &color_representation, 16)){
                color_representation = 0; // Not found default to 0
            }


            // Test memory capacity and add to table
            if (color_count == color_capacity){
                size_t new_capacity = color_capacity ? color_capacity * 2 : 2;
                struct color_entry *temp = realloc(color_entries, new_capacity * sizeof(struct color_entry));
                if (!temp) { // Probably out of memory :(
                    free(color_entries);
                    lfs_file_close(&lfs, &lut_file);
                    return PICO_ERROR_INSUFFICIENT_RESOURCES;
                }
                color_entries = temp;
                color_capacity = new_capacity;
            }

            struct color_entry *color = &color_entries[color_count++];
            color->hue = (uint16_t)hue_ul;
            color->saturation = (uint8_t)sat_ul;
            color->value = (uint8_t)val_ul;
            color->color_name = color_name;
            color->led_color_representation = color_representation;

            i = 0;
        }
    }
    lfs_file_close(&lfs, &lut_file);

    color_lut.data = color_entries;
    color_lut.len = color_count;

    // Successfully read
    return 0;
}

int color_init() {
    #ifdef SISYFOSS_AUX_SENSOR_LED
    gpio_init(SISYFOSS_AUX_SENSOR_LED);
    gpio_set_dir(SISYFOSS_AUX_SENSOR_LED, GPIO_OUT);
    gpio_put(SISYFOSS_AUX_SENSOR_LED, true);
    #endif

    // Does not immediately return because LUT can be missing and that would leave a part of the device unitialized
    int err = color_lut_load();

    #ifdef SISYFOSS_COLOR_VEML3328
    err = veml3328_setup(sisyfoss_i2c_inst);
    #endif

    return err;
}

int color_read_sensor(color_measurement *color) {

    #ifdef SISYFOSS_AUX_SENSOR_LED
    gpio_put(SISYFOSS_AUX_SENSOR_LED, false); // Turn the AUX LED ON
    #endif

    #ifdef SISYFOSS_COLOR_VEML3328
    veml3328_shutdown(sisyfoss_i2c_inst, false); // Wake the sensor
    veml3328_trigger(sisyfoss_i2c_inst);
    #endif

    #ifdef SISYFOSS_AUX_SENSOR_LED
    sleep_ms(150); // So the light has time to integrate
    #endif

    #ifdef SISYFOSS_COLOR_VEML3328
    veml3328_read_color(sisyfoss_i2c_inst, color);
    #endif

    #ifdef SISYFOSS_AUX_SENSOR_LED
    gpio_put(SISYFOSS_AUX_SENSOR_LED, true);
    #endif

    #ifdef SISYFOSS_COLOR_VEML3328
    veml3328_shutdown(sisyfoss_i2c_inst, true); // Sleep the sensor
    #endif

    return PICO_OK;
}

struct color_entry* get_color_lut_entry(uint8_t index){
    if (color_lut.data != NULL || index < color_lut.len) {
        return &color_lut.data[index];
    }
    return NULL;
}

/*
 * Finds the closest LUT color to the one on input.
 *
 * max_dist is the maximum distance of those two colors.
 * min_clear is the lowest clear channel luminance that can still be considered a color.
 */
int color_lut_get_entry(color_measurement *color, struct color_matched_entry *output, int max_dist, uint8_t min_clear) {
    if (color_lut.data == NULL || color_lut.len == 0 || min_clear > color->clear) {
        return -1;
    }

    char best_name = 0;
    long best_representation = 0;

    int best_dist = INT_MAX;

    for (int i = 0; i < color_lut.len; i++) {
        struct color_entry *entry = &color_lut.data[i];

        int16_t hue_diff = entry->hue - color->hue;
        int16_t sat_diff = entry->saturation - color->saturation;
        int16_t val_diff = entry->value - color->value;

        // Wrap hue into [-180,180] so red actually works
        if (hue_diff > 180) hue_diff -= 360;
        if (hue_diff < -180) hue_diff += 360;

        // Squared distance because it's faster than euclidian
        // A simple sum of absolute values is not good for comparing colors (learned the hard way)
        int dist = hue_diff * hue_diff +
                   sat_diff * sat_diff +
                   val_diff * val_diff;

        if (dist < best_dist) {
            best_dist = dist;
            best_name = entry->color_name;
            best_representation = entry->led_color_representation;
        }
    }

    // Enforce threshold
    if (best_dist > max_dist) {
        return -1;
    }

    output->name = best_name;
    output->led_color_representation = best_representation;

    return 0;
}
