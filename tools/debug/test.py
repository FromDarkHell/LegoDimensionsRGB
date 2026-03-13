import argparse
import time
from gateway import Gateway


def main():
    gateway = Gateway()

    print("Switching all pads to 0xFFFFFF")
    gateway.switch_pad(0, (255, 255, 255))

    time.sleep(1)

    gateway.clear_pads()

    print("Flashing pad...")

    gateway.flash_pads(
        [
            (5, 10, 15, (255, 0, 0)),
            (20, 25, 30, (0, 255, 0)),
            (35, 40, 45, (0, 0, 255)),
        ]
    )

    time.sleep(5)

    print("Fading pad...")
    gateway.switch_pad(0, (255, 255, 255))
    time.sleep(1)

    gateway.fade_pads(
        [
            (5, 10, (255, 0, 0)),
            (20, 25, (0, 255, 0)),
            (35, 40, (0, 0, 255)),
        ]
    )

    time.sleep(5)

    gateway.clear_pads()


if __name__ == "__main__":
    main()
