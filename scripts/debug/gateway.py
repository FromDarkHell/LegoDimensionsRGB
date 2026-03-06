import array
import platform
from typing import List, Optional, Tuple
import usb.core


class Gateway:
    """Represents a Lego Dimensions gateway/portal peripheral"""

    # STARTUP_PACKET = array.array(
    #     "B",
    #     [
    #         0x55,
    #         0x0F,
    #         0xB0,
    #         0x01,
    #         0x28,
    #         0x63,
    #         0x29,
    #         0x20,
    #         0x4C,
    #         0x45,
    #         0x47,
    #         0x4F,
    #         0x20,
    #         0x32,
    #         0x30,
    #         0x31,
    #         0x34,
    #         0xF7,
    #         0x00,
    #         0x00,
    #         0x00,
    #         0x00,
    #         0x00,
    #         0x00,
    #         0x00,
    #         0x00,
    #         0x00,
    #         0x00,
    #         0x00,
    #         0x00,
    #         0x00,
    #         0x00,
    #     ],
    # )

    VENDOR_ID = 0x0E6F
    PRODUCT_ID = 0x0241
    MAX_LENGTH = 0x20

    def __init__(self, verbose: bool = True):
        self.verbose = verbose
        self.used_kernel_driver = False
        self._cid = 0

        # Initialise USB connection to the device
        self.dev = self._init_usb()

        # Reset the state of the device to all pads off
        self.clear_pads()

    def __del__(self):
        if self.used_kernel_driver:
            self.dev.attach_kernel_driver(0)

    def _init_usb(self):
        """
        Connect to and initialise the portal
        """
        # Let's try and find the USB Device

        # Vendor ID: 0x0E6F (Logic3)
        dev = usb.core.find(idVendor=Gateway.VENDOR_ID, idProduct=Gateway.PRODUCT_ID)

        # Double check that the device was found
        if dev is None:
            raise ValueError("Device not found")

        if platform.system() == "Linux":
            if dev.is_kernel_driver_active(0):
                dev.detach_kernel_driver(0)
                self.used_kernel_driver = True

        # Initialise portal
        if self.verbose:
            print(f"Found portal at port #{dev.port_number}")

        # Set the active configuration. With no arguments, the first
        # configuration will be the active one
        dev.set_configuration()
        self.dev = dev

        # Startup
        _ENCODED = "(c) LEGO 2014".encode("ascii")
        self._send_command([0xB0, self.get_cid(), *_ENCODED])
        response = dev.read(0x81, Gateway.MAX_LENGTH, timeout=1_000)
        print(f"{bytes(response).hex()}")

        return dev

    def _send_command(self, command: List[int]):
        # One byte must be left unfilled in order to fill the checksum
        assert len(command) <= 31

        def convert_to_packet(command: List[int]):
            if command[0] != 0x55:
                command.insert(0, 0x55)
                command.insert(1, len(command) - 1)

            assert len(command) <= 31

            checksum = 0
            for word in command:
                checksum += word
                if checksum >= 256:
                    checksum -= 256

            message = [*command, checksum]

            assert len(message) <= 32
            while len(message) < 32:
                message.append(0x00)

            return message

        packet = convert_to_packet(command)
        self.dev.write(1, packet)

    def connected(self) -> bool:
        dev = usb.core.find(idVendor=Gateway.VENDOR_ID, idProduct=Gateway.PRODUCT_ID)
        if dev == None:
            return False

        return dev.address == self.dev.address

    def clear_pads(self):
        self.switch_pad(pad_id=0, color=(0, 0, 0))

    def switch_pad(self, pad_id: int, color: Tuple[int, int, int]):
        """
        Changes the color of one (or all) pads immediately
        pad_id: 0 = All, 1 = Center, 2 = Left, 3 = Right
        Color values are clamped betwen 0 and 255, in RGB order
        """

        r, g, b = color
        command = [0xC0, self.get_cid(), pad_id, r, g, b]
        self._send_command(command)

    def flash_pad(
        self,
        pad_id: int,
        on_length: int,
        off_length: int,
        pulse_count: int,
        color: Tuple[int, int, int],
    ):
        r, g, b = color
        command = [
            0xC3,
            self.get_cid(),
            pad_id,
            on_length,
            off_length,
            pulse_count,
            r,
            g,
            b,
        ]

        self._send_command(command)

    def flash_pads(
        self, pad_data: List[Optional[Tuple[int, int, int, Tuple[int, int, int]]]]
    ):
        """
        Flashes all pads with their own individual colors and rates
        Each pad is represented by a tuple in the format:
            - `(on_length, off_length, pulse_count, (R, G, B))`
        Pads are in the order of: (center, left, right)
        Color values must be between 0 and 255 (0x00 - 0xFF).
        Use `None` to ignore flashing that specific pad. Ignored pads continue to do whatever they were doing previously.

        - `on_length`: A value of 0x00 is practically non-existant, and a value of 0xFF is ~25.5 seconds
        - `off_length`: A value of 0x00 is practically non-existant, and a value of 0xFF is ~25.5 seconds
        - `pulse_count`: An odd value leaves pad in new colour, even leaves pad in old, except for 0x00, which does nothing. Values above 0xc6 dont stop.
        """
        assert len(pad_data) == 3
        command = [0xC7, self.get_cid()]
        for pad in pad_data:
            enable, on, off, pulse = 0, 0, 0, 0
            r, g, b = 0, 0, 0
            if pad != None:
                enable = 1
                on, off, pulse, (r, g, b) = pad
            command += [enable, on, off, pulse, r, g, b]

        self._send_command(command)

    def fade_pad(
        self, pad_id: int, speed: int, count: int, color: Tuple[int, int, int]
    ):
        r, g, b = color
        command = [0xC2, self.get_cid(), pad_id, speed, count, r, g, b]

        self._send_command(command)

    def fade_pads(
        self, pad_data: List[Optional[Tuple[int, int, Tuple[int, int, int]]]]
    ):
        assert len(pad_data) == 3
        command = [0xC6, self.get_cid()]
        for pad in pad_data:
            enable, fade, count = 0, 0, 0
            r, g, b = 0, 0, 0

            if pad != None:
                enable = 1
                fade, count, (r, g, b) = pad

            command += [enable, fade, count, r, g, b]

        self._send_command(command)

    def get_cid(self):
        self._cid += 1
        return self._cid
