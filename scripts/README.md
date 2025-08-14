## Scripts

These are some simple little scripts which uses PyUSB (libusb) to control the colors of a Lego Dimensions RGB Pad (**Note**: only tested is PS3/Wii/Wii U gamepads).

### Installation

#### Windows
- You can install the python dependencies via `python -m pip install -r requirements.txt`
- In order to properly utilize the device via USB, you will need to install a new/different Windows driver for the Dimensions RGB Pad.
    * You can do this via [Zadig](https://zadig.akeo.ie/#), an easy USB driver installer
    * You will also need to install [libusb](https://libusb.info/)

### Usage

Once you've installed everything, you can run `python ./test.py` which will run through all of the valid color-changing functions.
If you know that it works, you can then run `python ./static-color.py --color "#FF00FF"` which will change the RGB color of the playpad. This script should always be runnin