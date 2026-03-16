#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <functional>
#include "lego/crypto/burtle.h"
#include "lego/crypto/tea.h"
#include "lego/hid/packets.h"
#include "lego/hid/pad.h"
#include "lego/tag.h"
#include "lego/util/event_queue.h"
#include "log/logger.h"

#define PLAYPAD_VENDOR_ID 0x0E6F
#define PLAYPAD_PRODUCT_ID 0x0241
#define PLAYPAD_PRODUCT "LEGO READER V2.10"
#define PLAYPAD_MANUFACTURER "PDP LIMITED. "
#define PLAYPAD_SERIAL "P.D.P.000000"
#define PLAYPAD_VERSION 0x0100
#define PLAYPAD_WRITE_ENDPOINT 0x0001
#define PLAYPAD_READ_ENDPOINT 0x0081

static const uint8_t desc_hid_report[] = {
    0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,        // Usage (0x01)
    0xA1, 0x01,        // Collection (Application)

    0x19, 0x01,        //   Usage Minimum (1)
    0x29, 0x20,        //   Usage Maximum (32)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x20,        //   Report Count (32)
    0x81, 0x00,        //   Input  (Array)

    0x19, 0x01,  //   Usage Minimum (1)
    0x29, 0x20,  //   Usage Maximum (32)
    0x91, 0x00,  //   Output (Array)

    0xC0  // End Collection
};

static_assert(sizeof(desc_hid_report) == 29,
              "HID report descriptor must be exactly 29 bytes to match OEM wDescriptorLength");

class PlayPad {
   public:
    PlayPad();

    /**
     * @brief Initializes the PlayPad device and configures the USB HID interface. This should be
     * called in the setup() function of the main program. After calling this function, the device
     * should be ready to send and receive HID reports with the host.
     *
     */
    void begin();

    /**
     * @brief Updates various playpad state info (i.e. Flashing/Fading LEDs)
     *
     */
    void update();

    PlaypadPad* getPad(PadLocation loc) {
        if (loc == PadLocation::ALL) {
            return nullptr;
        }

        return &PADS[static_cast<uint8_t>(loc) - 1];
    }

    inline void sendPacket(BasePacket& packet) {
        log_dbg("[Playpad] Sending packet data: %s", packet.toHexString());

        uint8_t buffer[PLAYPAD_MAX_PACKET_SIZE];
        memcpy(buffer, packet.data, PLAYPAD_MAX_PACKET_SIZE);
        _sendReport(buffer, PLAYPAD_MAX_PACKET_SIZE);
    }

    inline void sendPacket(uint8_t* packet) {
        static char hexString[PLAYPAD_MAX_PACKET_SIZE * 2 + 1];
        for (size_t i = 0; i < PLAYPAD_MAX_PACKET_SIZE; ++i) {
            sprintf(&hexString[i * 2], "%02X", packet[i]);
        }

        log_dbg("[Playpad] Sending RAW packet data: %s", hexString);

        uint8_t buffer[PLAYPAD_MAX_PACKET_SIZE];
        memcpy(buffer, packet, PLAYPAD_MAX_PACKET_SIZE);
        _sendReport(buffer, PLAYPAD_MAX_PACKET_SIZE);
    }

    /**
     * @brief Fires the appropriate HID tag-event toward the game.
     *        If padIndex == TagIndex::Unplaced the tag is treated as
     *        removed; any other valid pad location fires a placement event.
     *
     * @param tag  The tag that was placed or lifted. padIndex must already
     *             reflect the new location before this is called.
     */
    void tagChangeEvent(ToyTag* placedTag, TagIndex lastLocation);

    /**
     * @brief Registers a callback that fires whenever any pad's display state
     *        changes (color, fade, flash).
     *
     * @param cb  Called with a pre-serialized JSON string of the full pad state.
     *            Invoked from the main loop (update()), never from a USB callback.
     */
    void onPadStateChange(std::function<void(const String&)> cb) { _padStateCallback = cb; }

   private:
    /**
     * @brief The Adafruit_USBD_HID instance that manages the USB HID interface. This is configured
     * to use the report descriptor defined above, and will handle the USB communication with the
     * host.
     *
     */
    Adafruit_USBD_HID _usb_hid;

    /**
     * @brief A simple boolean state for whether or not we're already sending a packet, this
     * shouldn't be too important in practice.
     *
     */
    bool _isSending = false;

    /**
     * @brief Configures the USB HID device with the correct vendor ID, product ID, and other device
     * properties.
     *
     */
    void _configureDevice();

    /**
     * @brief Reenumerates the USB device to make sure the host recognizes any changes in device
     * configuration.
     *
     */
    void _reenumerate();

    /**
     * @brief Sends a HID report to the host. The buffer should already be in the format expected by
     * the PlayPad, so the caller is responsible for constructing the packet correctly.
     *
     * @param buffer The buffer containing the HID report to send. This should already be in the
     * format expected by the PlayPad, including any necessary headers, checksums, etc.
     * @param bufsize Size of the buffer to send. This should not exceed PLAYPAD_MAX_PACKET_SIZE.
     * @return true
     * @return false
     */
    bool _sendReport(uint8_t const* buffer, uint16_t bufsize);

    /**
     * @brief Static callback function that gets called when the host requests a HID report. This
     * function should fill the provided buffer with the requested report data, and return the
     * length of the report. The report_id and report_type parameters can be used to determine which
     * report is being requested, if multiple reports are supported.
     *
     * @param report_id
     * @param report_type
     * @param buffer
     * @param reqlen
     * @return uint16_t
     */
    static uint16_t _getReportCallback(uint8_t report_id,
                                       hid_report_type_t report_type,
                                       uint8_t* buffer,
                                       uint16_t reqlen);

    /**
     * @brief Static callback function that gets called when the host sends a HID report to the
     * device. This function should process the received report data as needed. The report_id and
     * report_type parameters can be used to determine which report is being sent, if multiple
     * reports are supported.
     *
     * @param report_id
     * @param report_type
     * @param buffer
     * @param bufsize
     */
    static void _setReportCallback(uint8_t report_id,
                                   hid_report_type_t report_type,
                                   uint8_t const* buffer,
                                   uint16_t bufsize);

    /**
     * @brief Handles any command packet, and routes it to the specific command handler function
     *
     * @param packet
     */
    void _handleCommandPacket(const CommandPacket& packet);

    using CommandHandler = std::function<void(const CommandPacket&)>;
    std::unordered_map<GatewayCommand, CommandHandler> _commandHandlers;

    /**
     * @brief Registers and initializes all of the command handlers for various commands
     *
     */
    void _registerHandlers();
    void _handleWake(const CommandPacket& packet);
    void _handleSeed(const CommandPacket& packet);
    void _handleChallenge(const CommandPacket& packet);
    void _handleCol(const CommandPacket& packet);
    void _handleColAll(const CommandPacket& packet);
    void _handleFlash(const CommandPacket& packet);
    void _handleFlashAll(const CommandPacket& packet);
    void _handleFade(const CommandPacket& packet);
    void _handleFadeAll(const CommandPacket& packet);

    void _handleGetColor(const CommandPacket& packet);

    void _handleTagList(const CommandPacket& packet);
    void _handleRead(const CommandPacket& packet);
    void _handleModel(const CommandPacket& packet);

    void _handleWrite(const CommandPacket& packet);

    PlaypadPad PADS[3] = {
        PlaypadPad(PadLocation::LEFT),
        PlaypadPad(PadLocation::CENTER),
        PlaypadPad(PadLocation::RIGHT),
    };

    Burtle _prng;

    uint8_t TEA_KEY[16] = {0x55, 0xFE, 0xF6, 0xB0, 0x62, 0xBF, 0x0B, 0x41,
                           0xC9, 0xB3, 0x7C, 0xB4, 0x97, 0x3E, 0x29, 0x7B};
    TEA _crypto;

    EventQueue _eventQueue;

    std::function<void(const String&)> _padStateCallback = nullptr;
    bool _padStateDirty = false;

    /**
     * @brief An internal function used for updating the pad state after any major change
     * (colors/etc)
     *
     */
    void _notifyPadStateChange();

    /**
     * @brief A static pointer to the single instance of the PlayPad class. This is used in the
     * static callback functions to access the instance's member variables and functions, since the
     * callbacks themselves cannot be member functions.
     *
     */
    static PlayPad* _instance;

    /**
     * @brief A struct used for keeping track of placed tokens
     *
     */
    struct PlacedToken {
        uint8_t index;         // the index value sent in HID events
        TagIndex padLocation;  // pad number 1-3
        ToyTag* tag;           // non-owning pointer
    };

    static constexpr uint8_t MAX_TOKENS = 7;

    /**
     * @brief An array/pointer used for storing all of the tokens currently placed on the pad
     *
     */
    PlacedToken _placedTokens[MAX_TOKENS]{};

    /**
     * @brief The amount of currently placed tags inside of _placedTokens
     *
     */
    uint8_t _placedCount = 0x00;

    /**
     * @brief A bitmask for the currently available indexes
     *
     */
    uint8_t _indexMask = 0b00000000;  // bit N set → index N currently in use

    /**
     * @brief Allocates a new index to be used inside of the bitmask  (i.e. during tag placement)
     *
     * @return uint8_t The actual index byte-value
     */
    uint8_t _allocIndex() {
        for (uint8_t i = 0; i < MAX_TOKENS; i++) {
            if (!(_indexMask & (1u << i))) {
                _indexMask |= (1u << i);
                return i;
            }
        }
        return 0xFF;  // table full
    }

    /**
     * @brief Frees an index to be accessible for use (i.e. during tag removal)
     *
     * @param idx An index value to free up for future use
     */
    void _freeIndex(uint8_t idx) { _indexMask &= ~(1u << idx); }

    /**
     * @brief Finds a PlacedToken instance where the tag is equal to a tag placed on the
     *  playpad.
     *
     * @param tag The tag to find
     * @return PlacedToken* A tag found (or a `nullptr`)
     */
    PlacedToken* _findToken(const ToyTag* tag) {
        for (size_t i = 0; i < _placedCount; i++) {
            if (_placedTokens[i].tag == tag) {
                return &_placedTokens[i];
            }
        }

        return nullptr;
    }

    static PadLocation padSectionFromLocation(TagIndex loc) {
        switch (loc) {
            case TagIndex::TopLeft:
            case TagIndex::BottomLeft:
            case TagIndex::CenterLeft:
                return PadLocation::LEFT;
            case TagIndex::Middle:
                return PadLocation::CENTER;
            case TagIndex::TopRight:
            case TagIndex::BottomRight:
            case TagIndex::CenterRight:
                return PadLocation::RIGHT;
            default:
                return PadLocation::ALL;  // sentinel for "unknown"
        }
    }
};