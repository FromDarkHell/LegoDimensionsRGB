#pragma once

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "lego/hid/packets.h"
#include "log/logger.h"
#include "lego/crypto/burtle.h"
#include "lego/crypto/tea.h"
#include "lego/hid/pad.h"
#include <functional>

#define PLAYPAD_VENDOR_ID 0x0E6F
#define PLAYPAD_PRODUCT_ID 0x0241
#define PLAYPAD_PRODUCT "LEGO READER V2.10"
#define PLAYPAD_MANUFACTURER "PDP LIMITED. "
#define PLAYPAD_SERIAL "P.D.P.000000"
#define PLAYPAD_VERSION 0x0100
#define PLAYPAD_WRITE_ENDPOINT 0x0001
#define PLAYPAD_READ_ENDPOINT 0x0081

uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_GENERIC_INOUT(32)};

class PlayPad
{
public:
    PlayPad();

    /**
     * @brief Initializes the PlayPad device and configures the USB HID interface. This should be called in the setup() function of the main program. After calling this function, the device should be ready to send and receive HID reports with the host.
     *
     */
    void begin();

    /**
     * @brief Updates various playpad state info (i.e. Flashing/Fading LEDs)
     *
     */
    void update();

    PlaypadPad *getPad(PadLocation loc)
    {
        if (loc == PadLocation::All)
            return nullptr;

        return &PADS[static_cast<uint8_t>(loc) - 1];
    }

    inline void sendPacket(BasePacket &packet)
    {
        log_dbg("[Playpad] Sending packet data: %s", packet.toHexString());

        uint8_t buffer[PLAYPAD_MAX_PACKET_SIZE];
        memcpy(buffer, packet.data, PLAYPAD_MAX_PACKET_SIZE);
        _sendReport(buffer, PLAYPAD_MAX_PACKET_SIZE);
    }

    inline void sendPacket(uint8_t *packet)
    {
        static char hexString[PLAYPAD_MAX_PACKET_SIZE * 2 + 1];
        for (size_t i = 0; i < PLAYPAD_MAX_PACKET_SIZE; ++i)
            sprintf(&hexString[i * 2], "%02X", packet[i]);

        log_dbg("[Playpad] Sending RAW packet data: %s", hexString);

        uint8_t buffer[PLAYPAD_MAX_PACKET_SIZE];
        memcpy(buffer, packet, PLAYPAD_MAX_PACKET_SIZE);
        _sendReport(buffer, PLAYPAD_MAX_PACKET_SIZE);
    }

private:
    /**
     * @brief The Adafruit_USBD_HID instance that manages the USB HID interface. This is configured to use the report descriptor defined above, and will handle the USB communication with the host.
     *
     */
    Adafruit_USBD_HID _usb_hid;

    /**
     * @brief Configures the USB HID device with the correct vendor ID, product ID, and other device properties.
     *
     */
    void _configureDevice();

    /**
     * @brief Reenumerates the USB device to make sure the host recognizes any changes in device configuration.
     *
     */
    void _reenumerate();

    /**
     * @brief Sends a HID report to the host. The buffer should already be in the format expected by the PlayPad, so the caller is responsible for constructing the packet correctly.
     *
     * @param buffer The buffer containing the HID report to send. This should already be in the format expected by the PlayPad, including any necessary headers, checksums, etc.
     * @param bufsize Size of the buffer to send. This should not exceed PLAYPAD_MAX_PACKET_SIZE.
     * @return true
     * @return false
     */
    bool _sendReport(uint8_t const *buffer, uint16_t bufsize);

    /**
     * @brief Static callback function that gets called when the host requests a HID report. This function should fill the provided buffer with the requested report data, and return the length of the report. The report_id and report_type parameters can be used to determine which report is being requested, if multiple reports are supported.
     *
     * @param report_id
     * @param report_type
     * @param buffer
     * @param reqlen
     * @return uint16_t
     */
    static uint16_t _getReportCallback(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen);

    /**
     * @brief Static callback function that gets called when the host sends a HID report to the device. This function should process the received report data as needed. The report_id and report_type parameters can be used to determine which report is being sent, if multiple reports are supported.
     *
     * @param report_id
     * @param report_type
     * @param buffer
     * @param bufsize
     */
    static void _setReportCallback(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize);

    /**
     * @brief Handles any command packet, and routes it to the specific command handler function
     *
     * @param packet
     */
    void _handleCommandPacket(const CommandPacket &packet);

    using CommandHandler = std::function<void(const CommandPacket &)>;
    std::unordered_map<GatewayCommand, CommandHandler> _commandHandlers;

    /**
     * @brief Registers and initializes all of the command handlers for various commands
     *
     */
    void _registerHandlers();
    void _handleWake(const CommandPacket &packet);
    void _handleSeed(const CommandPacket &packet);
    void _handleChallenge(const CommandPacket &packet);
    void _handleCol(const CommandPacket &packet);
    void _handleColAll(const CommandPacket &packet);
    void _handleFlash(const CommandPacket &packet);
    void _handleFlashAll(const CommandPacket &packet);
    void _handleFade(const CommandPacket &packet);
    void _handleFadeAll(const CommandPacket &packet);

    PlaypadPad PADS[3] = {
        PlaypadPad(PadLocation::Left),
        PlaypadPad(PadLocation::Center),
        PlaypadPad(PadLocation::Right),
    };

    Burtle _prng;

    uint8_t TEA_KEY[16] = {
        0x55, 0xFE, 0xF6, 0xB0, 0x62, 0xBF, 0x0B, 0x41, 0xC9, 0xB3, 0x7C, 0xB4, 0x97, 0x3E, 0x29, 0x7B};
    TEA _crypto;

    /**
     * @brief A static pointer to the single instance of the PlayPad class. This is used in the static callback functions to access the instance's member variables and functions, since the callbacks themselves cannot be member functions.
     *
     */
    static PlayPad *_instance;
};