## LegoDimensionsRGB

This project is a reverse-engineered conversion of a Lego Dimensions (PS3 / Wii U) playpad, in order to keep the respective playpad lights consistently-on.  
It's powered using an ESP32-S3 which lets you change the color easily via a simple web frontend.  

### Project Layout

There's multiple (planned) board versions; The main / simplest one is `basic-rgb` which allows you to use the ESP32-S3 to control the Playpad's LEDs via a web interface.  

- `firmware` contains the PlatformIO project for the various board versions
- `schematic` contains the KiCad PCB projects for the various board versions.
- `scripts` contains a folder with various Python scripts used for controlling an authentic Lego Dimensions Playpad.
- `reversing` contains another KiCad project which has a vaguely correct, reverse engineered PCB of an original PS3 / Wii U Playpad

#### basic-rgb

This is a basic board which uses the ESP32-S3's D-/D+ lines to connect to a playpad and send basic color change commands.  
![A Lego Dimensions Playpad lit up powered by an ESP32-S3](/firmware/basic-rgb/photos/01.jpg)  
For more info, go to the [basic-rgb](/firmware/README.md#basic-rgb)

### Credits
This project is predominantly based on [woodenphone/lego_dimensions_protocol](https://github.com/woodenphone/lego_dimensions_protocol/blob/master/lego_dimensions_gateway.py). The main difference is that I have updated it to support Python 3 as well as added a `connected()` status function to determine if the playpad was disconnected.

### Support

If you for some reason want to support me financially for this project (or others I make), you can donate to me via [ko-fi](https://ko-fi.com/fromdarkhell) or [Patreon](https://patreon.com/fromdarkhell).

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/O4O44GLCD) [![Support me on Patreon](https://img.shields.io/endpoint.svg?url=https%3A%2F%2Fshieldsio-patreon.vercel.app%2Fapi%3Fusername%3Dfromdarkhell%26type%3Dpatrons&style=for-the-badge)](https://patreon.com/fromdarkhell)