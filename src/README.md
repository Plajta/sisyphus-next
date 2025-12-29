# Sisyphus firmware
And RP2040 firmware built mainly for SisyFOSS boards. Includes a filesystem, color processing, USB stack with a custom protocol and a keyboard driver!

As this firmware is mainly built for custom boards it doesn't provide a good experience on the default Pico, but it still can run there.

A linker script is used for building, that allowed us to make a separate part of the flash JUST for the LittleFS filesystem. This means there is no way for data loss when upgrading firmware!

This repository consists of the following:
- [Audio](audio/) - takes care of audio and it's loading - built from [RPi's code](https://github.com/raspberrypi/pico-playground/blob/master/audio/sine_wave/sine_wave.c)
- [Battery management](battery/) - drivers for BMS chips
- [Boards](boards/) - headers for supported boards
- [Color](color/) - drivers for color sensors, color management, color LUT
- [Eternity](https://github.com/Plajta/eternity) - submodule - our custom built 3rd stage bootloader for the RP2040
- [Keyboard](keyboard/) - drivers for keyboard driver ICs
- [LittleFS](https://github.com/littlefs-project/littlefs) - submodule - an amazing small fail-safe filesystem for microcontrollers.
- [Protocol](protocol/) - a custom protocol for file management, diagnostics and setup
- [USB](usb/) - just TinyUSB headers
- [Util](util/) - misc stuff, setup for LittleFS for the RP2040, ARGB LED color indicator

## Installation
If you want to build the firmware yourself look [here](#building).

Github actions automatically builds the newest commit in three different versions:
1. Pico - meant for the RPi Pico - mainly for testing
2. Pico + Eternity - Also for the Pico, but includes our bootloader Eternity - also for testing
3. SisyFOSS - meant for the SisyFOSS β board, already includes Eternity
To install the normal Pico firmware just flash the `sisyphus.uf2`.

Builds with included Eternity can be flashed in two ways:
1. By flashing `combined.uf2` the normal way.
2. If your board already has Eternity on it, you can flash the `sisyphus.bin` file using the web interface included in this repository (WIP) or with [Eternity's flasher](https://github.com/Plajta/eternity/blob/main/protocol/flasher.py).

## Usage
The primary way of interacting with this firmware is using a [web interface](https://lithos.plajta.eu/) ([code](https://github.com/Plajta/lithos)).

If you want to take a look inside or just want to do it yourself, look inside in the [Protocol](protocol/) directory for its README and a bunch of interesting scripts.

To manually insert audio files, you also need to understand how the device chooses which file to play.
When a user presses a button on the device's keyboard, the firmware tries to play a file from its filesystem by filling in the following name template:
```
<color>_<row>_<column>.wav
```
The `color` field is an ASCII letter obtained from the color LUT after a color measurement. For more info, see the [color](color/) directory.

The `row` and `column` fields correspond to the button’s coordinates in the keyboard matrix, with the first button located at row 0 and column 0.

## Building
Building was only tested on Linux, it might work elsewhere but only to a limited extent.

If you don't already know what software you need to build a pico-sdk project you can take a look at RPi's excellent docs [1](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf), [2](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html#quick-start-your-own-project), for parts that talk about installing the toolchain.

First make sure you have your git submodules up to date. This will automatically clone [Eternity](https://github.com/Plajta/eternity)'s and [LittleFS](https://github.com/littlefs-project/littlefs)'s code.
```bash
git submodule update --init --recursive
```

Now create a `build` directory:
```bash
mkdir build
```
This is already where it gets a little bit complicated but stick with me. Since this firmware is mainly built for SisyFOSS boards we have to specifically say we want to build the firmware for them.
In the example below I picked the Sisyfoss β board. If you also want to include Eternity you have to specifically tell `cmake` to do that. Of course if you don't want either of those things just delete them.
```bash
cmake -DPICO_BOARD=plajta_sisyfoss_beta -DUSE_ETERNITY=ON ..
```
This should produce a bunch of binaries, but the ones you are interested in are either `sisyphus.uf2` or `combined.uf2` with `sisyphus.bin`. Take a look at how to work with them in the [Installation](#installation) part.
