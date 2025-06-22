import argparse
import struct
import time
import traceback
from gateway import Gateway


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--color", type=str, default="#FFFFFF")
    parser.add_argument("--delay", type=float, default=(30 * 60))

    args = parser.parse_args()

    hex_color = args.color.strip("#")
    color_tuple = struct.unpack("BBB", bytes.fromhex(hex_color))

    while True:
        try:
            gateway = Gateway(verbose=True)

            while True:
                gateway.switch_pad(0, color_tuple)
                time.sleep(args.delay)

            del gateway
        except KeyboardInterrupt:
            print("Process interrupted by user (Ctrl+C). Exiting gracefully.")
            break
        except:
            traceback.print_exc()


if __name__ == "__main__":
    main()
