import argparse
import threading
import time
from gateway import Gateway


def main():
    gateway = Gateway()

    if False:
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
                (10, 2, (255, 0, 0)),
                (15, 4, (0, 255, 0)),
                (25, 5, (0, 0, 255)),  # Right Pad = Stays Blue
            ]
        )

    gateway.fade_random(50, 2)
    gateway.sniff()


if __name__ == "__main__":
    main()
