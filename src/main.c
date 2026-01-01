#include <hardware/gpio.h>
#include <pico/error.h>
#include <pico/time.h>
#include <stdbool.h>
#include <stdio.h>
#include <bsp/board_api.h>
#include "bq25619.h"
#include "class/cdc/cdc_device.h"
#include "pico/binary_info.h" // IWYU pragma: keep
#include <tusb.h>
#include "protocol.h"
#include "hardware/i2c.h"
#include "audio.h"
#include "color.h"
#include "tca8418.h"
#include "ws2812.h"
#include "lightshow.h"

#include "littlefs-pico.h"

#define BUTTON_PIN 14

#ifdef SISYFOSS_I2S_ENABLE
bi_decl(bi_1pin_with_name(SISYFOSS_I2S_ENABLE, "I2S ENABLE"));
#endif

#if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
#error Sisyphus requires a board with I2C pins
#endif

lfs_t lfs;

volatile bool wakeup = false;
volatile uint8_t button_index = 0;
#ifdef SISYFOSS_LID_DETECT
volatile bool lid_closed = false;
#endif

volatile bool trigger_color_scan = false;
volatile bool trigger_audio = false;

static repeating_timer_t usb_timer;

struct color_matched_entry matched_color;
bool matched_color_valid = false;
uint8_t audio_gain_shift_index = 0; // Should be clamped to ~6, indicates right shift of gain

i2c_inst_t *sisyfoss_i2c_inst = i2c_default;

static bool usb_background_task(repeating_timer_t *rt) {
    (void) rt;
    tud_task();  // TinyUSB background task
    return true; // Keep repeating
}

// Taken out so we can call it before loop and on trigger
void scan_color(){
    color_measurement color;
    color_read_sensor(&color);
    // 170 is measured specifically for Sisyfoss's experimental 3D model right now, should be treated as a test number
    matched_color_valid = (color_lut_get_entry(&color, &matched_color, 100, 170) == PICO_OK);

    #ifdef PICO_DEFAULT_WS2812_PIN
    if (matched_color_valid){
        static lightshow_quartic_fade_state_t lightshow_state;

        lightshow_state.t = 0;
        lightshow_state.duration = 0.5;
        lightshow_state.reverse = false;
        lightshow_state.start_next_reversed = true;
        lightshow_state.base_color = matched_color.led_color_representation;

        lightshow_fade_setup(&lightshow_state);
    }
    #endif
}

void keyboard_interrupt() {
    if(!tca8418_k_int_available(sisyfoss_i2c_inst)){
        return;
    }

    uint8_t kbd_events = tca8418_num_events(sisyfoss_i2c_inst);

    // Re-check INT_STAT here (important per documentation)
    if (!tca8418_k_int_available(sisyfoss_i2c_inst)) {
        return;
    }

    uint8_t value;
    bool pressed;

    while (tca8418_num_events(sisyfoss_i2c_inst) > 0) {
        tca8418_get_key_from_fifo(sisyfoss_i2c_inst, &value, &pressed);
        if (pressed && !tud_cdc_connected()) {
            #ifdef SISYFOSS_LID_DETECT
            if (lid_closed){
                button_index = value;
                wakeup = true;
                trigger_audio = true;
            }
            else { // Settings mode
                switch (value){
                    case 1: // Volume down
                        if (audio_gain_shift_index < 6)
                            audio_gain_shift_index++;
                        break;
                    case 2: // Volume up
                        if (audio_gain_shift_index > 0)
                            audio_gain_shift_index--;
                        break;
                    default:
                        break;
                }
            }
            #else
            button_index = value;
            wakeup = true;
            trigger_audio = true;
            #endif
        }
    }

    // Clear the KE_INT interrupt bit by writing 1
    tca8418_k_int_reset(sisyfoss_i2c_inst);
}

void gpio_irq_dispatcher(uint gpio, uint32_t events){
    if (gpio == SISYFOSS_LID_DETECT && (events & GPIO_IRQ_EDGE_FALL)) {
        wakeup = true;
        trigger_color_scan = true;
        lid_closed = true;
    }
    else if (gpio == SISYFOSS_LID_DETECT && (events & GPIO_IRQ_EDGE_RISE)) {
        matched_color_valid = false;
        lid_closed = false;
    }
    #ifndef SISYFOSS_HAS_KEYBOARD_CONTROLLER
    else if (gpio == BUTTON_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
        if(!tud_cdc_connected()){
            wakeup = true;
            button_index = 0;
        }
    }
    #else
    else if (gpio == SISYFOSS_KEYBOARD_INTERRUPT && (events & GPIO_IRQ_EDGE_FALL)) {
        keyboard_interrupt();
    }
    #endif
}

int main() {
    #ifdef PICO_DEFAULT_WS2812_PIN
    // First of all get the status LED, so errors can be shown
    int err = ws2812_init(PICO_DEFAULT_WS2812_PIN);
    hard_assert(err);
    #endif

    i2c_init(sisyfoss_i2c_inst, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));
    bq25619_init(sisyfoss_i2c_inst); // init charger before all due safety reasons

    tusb_init();
    // Run tud_task every millisecond just like pico_stdio would do
    add_repeating_timer_ms(1, usb_background_task, NULL, &usb_timer);

    #ifdef SISYFOSS_I2S_ENABLE
    gpio_init(SISYFOSS_I2S_ENABLE);
    gpio_set_dir(SISYFOSS_I2S_ENABLE, GPIO_OUT);
    gpio_put(SISYFOSS_I2S_ENABLE, 1);
    #endif

    err = pico_lfs_init(&lfs);

    if (err) {
        sleep_ms(5000);
        return 0; // Todo add a color indicated error
    }

    init_audio();
    err = color_init();

    gpio_set_irq_callback(&gpio_irq_dispatcher);

    #ifndef SISYFOSS_HAS_KEYBOARD_CONTROLLER
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    gpio_set_irq_enabled(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true);
    #else
    tca8418_init(sisyfoss_i2c_inst);
    tca8418_setup_keyboard(sisyfoss_i2c_inst, 0b1111, 0b1111);
    tca8418_setup_interrupt(&keyboard_interrupt);
    #endif

    #ifdef SISYFOSS_LID_DETECT
    gpio_init(SISYFOSS_LID_DETECT);
    gpio_set_dir(SISYFOSS_LID_DETECT, GPIO_IN);
    gpio_pull_up(SISYFOSS_LID_DETECT);
    gpio_set_irq_enabled(SISYFOSS_LID_DETECT, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    lid_closed = !gpio_get(SISYFOSS_LID_DETECT);
    #endif

    // Enable GPIO IRQs
    irq_set_enabled(IO_IRQ_BANK0, true);

    #ifdef SISYFOSS_LID_DETECT
    if (lid_closed)
        scan_color();
    #else
    scan_color();
    #endif


    while (true) {
        // Check for USB-CDC connection
        if (tud_cdc_connected()) {
            #ifdef SISYPHUS_DEBUG
            char buf[128];
            // Get the things you wanna debug here
            snprintf(buf, sizeof(buf), "DEBUG MODE ON, DO NOT SHIP!!!\r\n");
            tud_cdc_write_str(buf);
            tud_cdc_write_flush();
            snprintf(buf, sizeof(buf), "DEBUG DATA: \r\n"); //add your debug data here
            tud_cdc_write_str(buf);
            tud_cdc_write_flush();
            #endif
            protocol_loop();
        }

        // If not connected and no button pressed, go to sleep
        while (!tud_cdc_connected() && !wakeup) {
            __wfi(); // Low-power wait for interrupt
        }

        if (wakeup) {
            wakeup = false;

            if (trigger_color_scan) {
                trigger_color_scan = false;
                scan_color();
            }

            if(trigger_audio){
                trigger_audio = false;
                if (matched_color_valid && button_index != 0){
                    char filename[16]; // 16 should be enough
                    uint8_t buttom_column = (button_index % 10)-1;
                    uint8_t button_row = button_index / 10;
                    snprintf(filename, sizeof(filename), "%c_%d_%d.wav", matched_color.name, button_row, buttom_column);
                    play_audio(filename, AUDIO_MAX_GAIN >> audio_gain_shift_index); // shift index is clamped
                }
            }
        }
    }


    return 0;
}
