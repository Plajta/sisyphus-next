/**
 * This code contains parts of Raspberry Pi's sine_wave.c for I2S audio: https://github.com/raspberrypi/pico-playground/blob/master/audio/sine_wave/sine_wave.c
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "lfs.h"
#include "pico/audio.h"
#include "pico/binary_info.h" // IWYU pragma: keep
#include "pico/audio_i2s.h"
#include "audio.h"

bool file_open = false;
lfs_file_t audio_file;

#ifndef SISYFOSS_I2S_DIN
#define SISYFOSS_I2S_DIN PICO_AUDIO_I2S_DATA_PIN
#endif

#ifndef SISYFOSS_I2S_BIT_CLOCK
#define SISYFOSS_I2S_BIT_CLOCK PICO_AUDIO_I2S_CLOCK_PIN_BASE
#endif

#ifndef SISYFOSS_I2S_FRAME_CLOCK
#define SISYFOSS_I2S_FRAME_CLOCK PICO_AUDIO_I2S_CLOCK_PIN_BASE+1
#endif

bi_decl(bi_3pins_with_names(SISYFOSS_I2S_DIN, "I2S DIN", SISYFOSS_I2S_BIT_CLOCK, "I2S BCK", SISYFOSS_I2S_FRAME_CLOCK, "I2S LRCK"));

static struct audio_buffer_pool *ap;

void init_audio() {

    static audio_format_t audio_format = {
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .sample_freq = 22100,
        .channel_count = 1,
    };

    static struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 2
    };

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                                                      SAMPLES_PER_BUFFER); // todo correct size
    bool ok;
    const struct audio_format *output_format;

    struct audio_i2s_config config = {
        .data_pin = SISYFOSS_I2S_DIN,
        .clock_pin_base = SISYFOSS_I2S_BIT_CLOCK,
        .dma_channel = 0,
        .pio_sm = 0,
    };

    output_format = audio_i2s_setup(&audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    ok = audio_i2s_connect(producer_pool);
    assert(ok);
    audio_i2s_set_enabled(true);
    ap = producer_pool;
}

void play_audio(char* filename) {
    int8_t error;

    if (file_open){
        lfs_file_close(&lfs, &audio_file);
    }
    // Implement filename picking from button index
    file_open = true;
    error = lfs_file_open(&lfs, &audio_file, filename, LFS_O_RDONLY);
    if (error != LFS_ERR_OK) {
        return;
    }
    // For now skip metadata, will implement later
    lfs_file_seek(&lfs, &audio_file, 44, LFS_SEEK_SET);

    bool running = true;

    while (running) {
        if (wakeup) {
            lfs_file_close(&lfs, &audio_file);
            file_open = false;
            return;
        }
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *samples = (int16_t *) buffer->buffer->bytes;

        // This uint32_t should not overflow as sample count per buffer will never be that high
        uint32_t read_bytes = lfs_file_read(&lfs, &audio_file, samples, buffer->max_sample_count * sizeof(int16_t));
        buffer->sample_count = read_bytes / sizeof(int16_t);
        give_audio_buffer(ap, buffer);
        if (read_bytes <= 1) { // If for whatever reason the file didn't have an even number of bytes
            lfs_file_close(&lfs, &audio_file);
            file_open = false;
            return;
        }
    }
}
