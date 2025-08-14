## basic-rgb

This project is a simple ESP32-S3-based controller for (PS3/Wii U/PS4) LEGO Dimensions playpads.  
Note: **This project does not emulate a full playpad. It only provides remote control of the LED colors via a web interface.**

![](/firmware/basic-rgb/photos/02.jpg)  
![](/firmware/basic-rgb/photos/04.jpg)  


### Features

- Control the colors via a basic HTML webpage
- (Semi) real-time HTTP logs of USB packets / etc

### Hardware
- ESP32-S3 (i.e. [Seeed Studio XIAO ESP32-S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/))
- USB 2.0 Breakout Board ([Example](https://www.amazon.com/dp/B07W7XMV3W))

How To:
- Connect the USB breakout board D-/D+ pins to your ESP32  
- Splice a USB cable (or use a USB C/etc breakout board), and connect the VUSB and GND to the ESP32 *and* the USB Breakout Board.  

Additionally, this repo also includes a basic schematic & PCB which you can use as a guide, or as an easy to use wiring setup. Enjoy!  

### Usage

1. Flash the firmware onto your ESP32-S3
2. Wire your ESP32-S3 (see above)
3. Connect your ESP32-S3 to your power source
4. It will create a WiFi AP you can connect to named, `captive`.
    - Connect to `captive`, and then enter your WiFi credentials.
5. Wait for the reboot, and connect to your WiFi network.
6. Then you can access the playpad via `playpad.local` (via MDNS)
7. Start changing colors!