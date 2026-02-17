/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * Edited to suit SisyFOSS's board - Plajta 2025 - Václav Straka
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

// pico_cmake_set PICO_PLATFORM=rp2040

#ifndef _BOARDS_PLAJTA_SISYFOSS_BETA_H
#define _BOARDS_PLAJTA_SISYFOSS_BETA_H

// For board detection
#define PLAJTA_SISYFOSS_BETA

//------------- UART -------------//
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 0
#endif

#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 0
#endif

#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 1
#endif

//------------- LED -------------//
#ifndef PICO_DEFAULT_WS2812_PIN
#define PICO_DEFAULT_WS2812_PIN 21
#endif

//------------- I2C -------------//
#ifndef PICO_DEFAULT_I2C
#define PICO_DEFAULT_I2C 0
#endif

#ifndef PICO_DEFAULT_I2C_SDA_PIN
#define PICO_DEFAULT_I2C_SDA_PIN 24
#endif

#ifndef PICO_DEFAULT_I2C_SCL_PIN
#define PICO_DEFAULT_I2C_SCL_PIN 25
#endif

//----------- Battery -----------//

#ifndef SISYFOSS_PMIC_INT
#define SISYFOSS_PMIC_INT 18
#endif

#ifndef SISYFOSS_PMIC_MAX_CHARGE_CURRENT
// Depends on the battery current capacity
#define SISYFOSS_PMIC_MAX_CHARGE_CURRENT 500
#endif

#ifndef SISYFOSS_PMIC_MAX_CHARGE_VOLTAGE
#define SISYFOSS_PMIC_MAX_CHARGE_VOLTAGE 4200
#endif

#ifndef SISYFOSS_PMIC_MAX_PRECHARGE_CURRENT
#define SISYFOSS_PMIC_MAX_PRECHARGE_CURRENT 40
#endif

#ifndef SISYFOSS_PMIC_MAX_TERMINATION_CURRENT
#define SISYFOSS_PMIC_MAX_TERMINATION_CURRENT 60
#endif

#ifndef SISYFOSS_PMIC_MAX_DISCHARGE_CURRENT
#define SISYFOSS_PMIC_MAX_DISCHARGE_CURRENT 1000
#endif

#ifndef SISYFOSS_PMIC_MAX_INPUT_CURRENT
#define SISYFOSS_PMIC_MAX_INPUT_CURRENT 500
#endif

#ifndef SISYFOSS_PMIC_IGNORE_TS
#define SISYFOSS_PMIC_IGNORE_TS 0
#endif

//------------- Misc ------------//
#ifndef SISYFOSS_KEYBOARD_INTERRUPT
#define SISYFOSS_KEYBOARD_INTERRUPT 14
#endif

#ifndef SISYFOSS_KEYBOARD_RESET
#define SISYFOSS_KEYBOARD_RESET 15
#endif

#ifndef SISYFOSS_LID_DETECT
#define SISYFOSS_LID_DETECT 19
#endif

#ifndef SISYFOSS_AUX_SENSOR_LED
#define SISYFOSS_AUX_SENSOR_LED 20
#endif

//------------- I2S -------------//
#ifndef SISYFOSS_I2S_ENABLE
#define SISYFOSS_I2S_ENABLE 26
#endif

#ifndef SISYFOSS_I2S_DIN
#define SISYFOSS_I2S_DIN 27
#endif

#ifndef SISYFOSS_I2S_BIT_CLOCK
#define SISYFOSS_I2S_BIT_CLOCK 28
#endif

#ifndef SISYFOSS_I2S_FRAME_CLOCK
#define SISYFOSS_I2S_FRAME_CLOCK 29
#endif


//------------- FLASH -------------//

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2 // TEST IF STABLE
#endif
// THESE COMMENTS ACTUALLY DO STUFF, DO NOT DELETE
// pico_cmake_set_default PICO_FLASH_SIZE_BYTES = (16 * 1024 * 1024)
// pico_cmake_set_default LITTLEFS_SIZE_BYTES_STRING = (15 * 1024 * 1024)
// All boards have B1 RP2040
#ifndef PICO_RP2040_B0_SUPPORTED
#define PICO_RP2040_B0_SUPPORTED 0
#endif

#endif
