#pragma once

#include <Arduino.h>
#include "config/config.h"
#include "constants.h"
#include "driver/gpio.h"
#include "log/logger.h"
#include "packet.h"
#include "usb/usb_host.h"

/**
 * @brief Per-pad config for a flashPads() call.
 *        Set `enabled = false` to leave that pad unchanged (matches Python's `None`).
 */
struct PadFlashConfig {
    bool enabled = false;
    uint8_t onTime = 0;
    uint8_t offTime = 0;
    uint8_t count = 0;
    PadColor offColor = PadColor::Black();
};

/**
 * @brief Per-pad config for a fadePads() call.
 *        Set `enabled = false` to leave that pad unchanged (matches Python's `None`).
 */
struct PadFadeConfig {
    bool enabled = false;
    uint8_t speed = 0;
    uint8_t count = 0;
    PadColor color = PadColor::Black();
};

/**
 * @brief Manages the USB host connection to a physical LEGO Dimensions playpad
 *        and exposes a high-level command API.
 *
 *
 * Usage:
 *   gateway.begin();               // in setup()
 *   gateway.loop();                // in loop()
 *   gateway.switchPad(PadLocation::LEFT, PadColor::Red());
 */
class Toypad {
   public:
    /**
     * @brief Install the USB host stack and register the client.
     *        Call once from setup().
     */
    bool begin();

    /**
     * @brief Pump USB host events. Call every iteration of loop().
     */
    bool loop();

    bool isConnected() const { return _deviceHandle != nullptr; }

    // ----------------------------------------------------------------
    //  Pad commands  (mirror Python Toypad methods)
    // ----------------------------------------------------------------

    /// Set all pads to black immediately.
    void clearPads() { switchPad(PadLocation::ALL, PadColor::Black()); }

    /**
     * @brief Set one pad (or ALL pads) to a solid color.  COL command.
     * @param pad   Target pad; PadLocation::ALL sets every pad.
     * @param color Desired color.
     */
    void switchPad(PadLocation pad, PadColor color);

    /**
     * @brief Set each pad to a different solid color in one COLAL command.
     */
    void switchPads(PadColor center, PadColor left, PadColor right);

    /**
     * @brief Flash one pad between its current color and `offColor`.  FLASH command.
     * @param onTime   Ticks (≈100 ms each) the pad stays at its current color.
     * @param offTime  Ticks the pad shows `offColor`.
     * @param count    Number of full on/off cycles (0 = infinite).
     * @param offColor Color shown during the "off" phase.
     */
    void flashPad(PadLocation pad,
                  uint8_t onTime,
                  uint8_t offTime,
                  uint8_t count,
                  PadColor offColor);

    /**
     * @brief Flash each pad independently in one FLSAL command.
     *        Pass a config with `enabled = false` to leave that pad unchanged.
     */
    void flashPads(PadFlashConfig center, PadFlashConfig left, PadFlashConfig right);

    /**
     * @brief Fade one pad from its current color to `color`.  FADE command.
     * @param speed  Ticks per half-cycle.
     * @param count  Number of half-cycles (0 = infinite).
     * @param color  Target color.
     */
    void fadePad(PadLocation pad, uint8_t speed, uint8_t count, PadColor color);

    /**
     * @brief Fade each pad independently in one FADAL command.
     *        Pass a config with `enabled = false` to leave that pad unchanged.
     */
    void fadePads(PadFadeConfig center, PadFadeConfig left, PadFadeConfig right);

    /**
     * @brief Read pad colors from config and apply them.
     *        Called automatically on connect; also useful after a config change.
     */
    void initColors();

   private:
    static Toypad* _instance;

    ToypadPlatform _padPlatform;

    usb_host_client_handle_t _clientHandle = nullptr;
    usb_device_handle_t _deviceHandle = nullptr;
    uint8_t _cid = 0;

    /// Monotonically increasing command ID, wraps at 255.
    uint8_t _nextCID() { return ++_cid; }

    /// Submit a fully-built CommandPacket to the write endpoint.
    void _sendPacket(const CommandPacket& pkt);

    /// Called when the playpad is first detected; claims the interface and wakes it.
    void _startup();

    void _onDeviceConnected(uint8_t address);
    void _onDeviceDisconnected(usb_device_handle_t handle);

    static void _clientEventCallback(const usb_host_client_event_msg_t* msg, void* arg);
    static void _transferCallback(usb_transfer_t* transfer);
};

extern Toypad toypad;