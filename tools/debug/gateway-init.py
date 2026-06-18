import argparse
import threading
import time
from gateway import Gateway


def main():
    gateway = Gateway(platform="xbox_360")

    if True:
        gateway.switch_pad(0, (255, 0, 255))
    elif False:
        gateway.flash_pads(
            [
                (5, 2, 10, (255, 0, 0)),
                (5, 2, 15, (0, 255, 0)),  # Left Pad = Stays Green
                (5, 2, 20, (0, 0, 255)),
            ]
        )
    elif False:
        gateway.flash_pad(0, 5, 2, 10, (0, 255, 0))
    elif False:
        # Structure: PAD ID, speed, count, (red, green, blue)

        # This example fades the pad up from black -> red -> black in 5s (the full cycle takes 5s)
        gateway.fade_pad(0, 50, 2, (255, 0, 0))

        # This example fades the pad from black -> green -> black -> green -> black all over 10s
        gateway.fade_pad(0, 50, 4, (0, 255, 0))
    elif False:
        gateway.fade_pads(
            [
                (0, 1, (76, 32, 0)),  # Center Pad = Red
                (0, 1, (255, 255, 0)),  # Left Pad = Green
                (0, 1, (255, 255, 255)),  # Right Pad = Stays Blue
            ]
        )
    elif False:
        gateway.fade_pads(
            [
                (0, 1, (0, 0, 24)),  # Center Pad = Blue
                None,
                None,
            ]
        )

        time.sleep(5)

        gateway.fade_pads(
            [
                (0, 1, (0, 0, 255)),  # Center Pad = Blue
                None,
                None,
            ]
        )

    # gateway.sniff()


if __name__ == "__main__":
    main()
