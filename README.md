## LegoDimensionsRGB

This project is a reverse-engineered implementation of a LEGO Dimensions (currently *only* PS3 / Wii U) playpad.  
This project also includes a simpler project which communicates with an OEM playpad to act as a lamp/night-light, as well as a Python script for communication as well.

### Project Layout
Inside of `projects`, you can see a few different sub-projects which each have their own purpose.

- [basic-emulator](/projects/basic-emulator/) is an implementation of the playpad HID spec, running on a Raspberry Pi Pico W, alongside a Web UI for easy controls.
- [rgb-controller](/projects/rgb-controller/) is a simpler project which sends basic USB HID commands *to* an existing, stock playpad for use as a lamp. This is powered by an ESP32-S3, and comes with a simple Web UI for controlling colors.

### Credits

A large portion of the emulator project is based on [AlinaNova21/node-ld](https://github.com/AlinaNova21/node-ld/tree/master), as well as [Berny23/LD-Toypad-Emulator](https://github.com/Berny23/LD-ToyPad-Emulator).

The Python tools is predominantly based on [woodenphone/lego_dimensions_protocol](https://github.com/woodenphone/lego_dimensions_protocol/blob/master/lego_dimensions_gateway.py). The main difference is that I have updated it to support Python 3 as well as added a `connected()` status function to determine if the playpad was disconnected.

### Support

If you for some reason want to support me financially for this project (or others I make), you can donate to me via [ko-fi](https://ko-fi.com/fromdarkhell) or [Patreon](https://patreon.com/fromdarkhell).

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/O4O44GLCD) [![Support me on Patreon](https://img.shields.io/endpoint.svg?url=https%3A%2F%2Fshieldsio-patreon.vercel.app%2Fapi%3Fusername%3Dfromdarkhell%26type%3Dpatrons&style=for-the-badge)](https://patreon.com/fromdarkhell)