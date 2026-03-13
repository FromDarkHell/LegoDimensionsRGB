#pragma once

#include "driver/gpio.h"
#include "log/logger.h"
#include "playpad_hid.h"
#include "usb/usb_host.h"

#include <string.h>
#include "config/config.h"

/// @brief Initializes a USB playpad over the internal USB data lines
/// @return Whether or not this was a success
bool playpad_init();

/// @brief Runs event handling for a USB playpad (i.e. USB unplugged, etc)
/// @return Whether or not this was a success
bool playpad_loop();

/// ==== Playpad Interaction / Commands ====

/// @brief Initializes all of the pad colors to the specific settings saved in the config
/// @return Whether or not this was a success
esp_err_t initialize_pad_colors();

/// @brief Changes multiple pads to their specific color
/// @param  left The specific color for the left-facing pad
/// @param  middle The specific color for the middle-facing pad
/// @param  right The specific color for the right-facing pad
/// @return Whether or not this was a success
esp_err_t change_pad_colors(playpad_color, playpad_color, playpad_color);

/// @brief Changes an individual pad to a specific color
/// @param pad The specific pad to color change
/// @param color The specific color to change to
/// @return Whether or not this was a success
esp_err_t change_pad_color(playpad_pad, playpad_color);