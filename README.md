## LegoDimensionsRGB

This project uses PyUSB (libusb) to control the colors of a Lego Dimensions RGB Pad (**Note**: only tested is PS3/Wii/Wii U gamepads).

### Installation

#### Windows
- You can install the python dependencies via `python -m pip install -r requirements.txt`
- In order to properly utilize the device via USB, you will need to install a new/different Windows driver for the Dimensions RGB Pad.
    * You can do this via [Zadig](https://zadig.akeo.ie/#), an easy USB driver installer
    * You will also need to install [libusb](https://libusb.info/)


#### Linux

TODO lol


### Usage

Once you've installed everything, you can run `python ./test.py` which will run through all of the valid color-changing functions.
If you know that it works, you can then run `python ./static-color.py --color "#FF00FF"` which will change the RGB color of the playpad. This script should always be running on your host device and will await for any disconnect/reconnections and reset the color on that connection


### Support

If you for some reason want to support me financially for this project (or others I make), you can donate to me via [ko-fi](https://ko-fi.com/fromdarkhell) or [Patreon](https://patreon.com/fromdarkhell).

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/O4O44GLCD) [![Support me on Patreon](https://img.shields.io/endpoint.svg?url=https%3A%2F%2Fshieldsio-patreon.vercel.app%2Fapi%3Fusername%3Dfromdarkhell%26type%3Dpatrons&style=for-the-badge)](https://patreon.com/fromdarkhell)