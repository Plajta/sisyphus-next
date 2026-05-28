# Sisyphus Firmware
An RP2040 firmware built primarily for SisyFOSS boards. It includes a filesystem, color processing, a USB stack with a custom protocol, and a keyboard driver!

While this firmware is designed for custom boards, it can still run on a standard Raspberry Pi Pico, though the experience may be limited.

A custom linker script is used during the build process to reserve a dedicated section of the flash JUST for the LittleFS filesystem. This ensures that data is preserved when upgrading the firmware.

## Repository Structure
- [Audio](audio/) - Manages audio playback and loading - built from [RPi's code](https://github.com/raspberrypi/pico-playground/blob/master/audio/sine_wave/sine_wave.c)
- [Battery management](battery/) - Drivers for BMS chips
- [Boards](boards/) - Headers for supported boards
- [Color](color/) - Drivers for color sensors, color management, and color LUT
- [Eternity](https://github.com/Plajta/eternity) - Submodule for our custom 3rd-stage bootloader for the RP2040
- [Keyboard](keyboard/) - Drivers for keyboard controller ICs
- [LittleFS](https://github.com/littlefs-project/littlefs) - Submodule for an amazing and small fail-safe filesystem for microcontrollers.
- [Protocol](protocol/) - A custom protocol for file management, diagnostics, and configuration
- [USB](usb/) - TinyUSB configuration and descriptors
- [Util](util/) - Misc stuff, setup for LittleFS for the RP2040, ARGB LED color indicator

## Installation
To build the firmware from source, refer to the [Building](#building) section.

GitHub Actions automatically build the latest commits in three versions:
1. **Pico**: Standard firmware for the Raspberry Pi Pico (primarily for testing).
2. **Pico + Eternity**: Firmware for the Pico including the Eternity bootloader (also for testing).
3. **SisyFOSS**: Firmware for the SisyFOSS β board, including Eternity.

To install the standard Pico firmware, flash `sisyphus.uf2`.

Builds that include Eternity can be flashed in two ways:
1. Flash `combined.uf2` using the standard method.
2. If Eternity is already installed on the board, flash the `sisyphus.bin` file using the [web interface](https://lithos.plajta.eu/) (still WIP) or the [Eternity flasher](https://github.com/Plajta/eternity/blob/main/protocol/flasher.py).

## Usage
The primary way to interact with the firmware is through the [web interface](https://lithos.plajta.eu/) ([code](https://github.com/Plajta/lithos)).

If you want to take a look inside or just like to do stuff yourself, look inside in the [Protocol](protocol/) directory for its README and a bunch of interesting scripts.

To manually insert audio files, you also need to understand how the device chooses which file to play.
When a button is pressed, the firmware tries to play a file from its filesystem based on the following name template:
```
<color>_<row>_<column>.wav
```
- `<color>` - An ASCII letter mapped from the color LUT. See the [color](color/) directory for details.
- `<row>` and `<column>` - The coordinates of the button in the keyboard matrix (starting from 0).

### Initial Device Setup
When setting up a new or reset device with an empty filesystem, a few files must be prepared.

#### Color Lookup Table (LUT)
The LUT maps sensor readings to specific color codes and LED displays. To generate a stable LUT, run the [calibrator.py](protocol/calibrator.py) script with the device connected. The script will guide you through the calibration process using color samples, make sure to prepare those from the printer you plan on using in all the supported colors.

Currently supported colors:
- Red
- Green
- Blue
- Cyan
- Magenta
- Yellow
- White

For more information on color detection, see to the [color](color/) directory.

#### Volume Feedback Sample
An optional but recommended file, `volume_sample.wav`, provides audible feedback when the user adjusts the device volume. It should be a short chime representative of the volume of your typical audio samples.

The easiest way to prepare and upload this chime is using the [convert.py](protocol/convert.py) script:
```bash
python protocol/convert.py <path_to_audio_sample> volume_sample.wav
```
This script automatically converts the sample to 16-bit PCM at 22 kHz and uploads it to the device.

Alternatively, you can use a 3rd-party tool like Audacity to prepare the file and upload it via the [web interface](https://lithos.plajta.eu/) or the [shell.py](protocol/shell.py) script.

## Building
Building was only tested on Linux, it might work elsewhere but only to a limited extent.

If you are new to `pico-sdk` development, refer to the official Raspberry Pi documentation ([Getting Started](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf), [SDK Quick Start](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html#quick-start-your-own-project)) for toolchain installation instructions.

1. **Update Submodules**: Ensure all submodules (Eternity and LittleFS) are cloned and up to date.
   ```bash
   git submodule update --init --recursive
   ```

2. **Create Build Directory**:
   ```bash
   mkdir build
   cd build
   ```

3. **Configure and Build**:
   Since the firmware is optimized for SisyFOSS boards, you should specify the board type. To include Eternity, use the `USE_ETERNITY` flag.
   ```bash
   cmake -DPICO_BOARD=plajta_sisyfoss_beta -DUSE_ETERNITY=ON ..
   make
   ```
   This should produce a bunch of binaries, but the ones you are interested in are either `sisyphus.uf2` (standard) or `combined.uf2` and `sisyphus.bin` (for Eternity-based systems). See the [Installation](#installation) section for deployment details.
