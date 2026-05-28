# Sisyphus
Sisyphus is an open-source, customizable soundboard. It's built to help folks who need assistive tech, but it's also great if you just want a cool device that makes noises when you hit buttons.

This repo contains everything you need to build one: the PCB designs, the 3D-printable case, and the firmware that makes it all work, with a construction manual in the works.

If you're looking for the web configurator to manage your sounds, check out [Lithos](https://lithos.plajta.eu/) (source code [here](https://github.com/Plajta/lithos)).

## Key features
With **RP2040** as it's beating heart and a nice crisp **I2S DAC**, it can even do CD quality audio (if you're into that kinda thing).
All the audio is kept in the device's **internal flash** held together by LittleFS making it resilient.

Made with our custom **[Eternity](https://github.com/Plajta/eternity) bootloader** it can be updated even from a browser. 

A built in **color sensor** makes it easy to swap your sound sets by just swapping in a piece of paper and **magnets** make it all feel like magic.
Built in a sturdy, slim, and light **3D-printed body**, held together by friction and screws for easy repairs. (Construction manual coming soon™)

## Directory structure
- [3D/](3D/) - The mechanical parts. Contains the FreeCAD project and [STLs](3D/STLs) ready for printing.
- [PCBs/](PCBs/) - KiCAD projects for the [mainboard](PCBs/sisyphus) and our own slim [RGB sensor module](PCBs/rgb_sensor).
- [src/](src/) - The firmware. It handles everything from the USB stack and keyboard matrix to the audio playback. Check its [README](src/README.md) for more details.

## Licensing
We take open-source seriously, so each part of the project has its own license to keep it fair:
- **3D & PCBs:** [CC-BY-SA](https://creativecommons.org/licenses/by-sa/4.0/)
- **Firmware (src):** [GPLv3](https://www.gnu.org/licenses/gpl-3.0.html)

That means you're free to use, modify, and even sell this—as long as you share your changes back with the community under the same terms.
