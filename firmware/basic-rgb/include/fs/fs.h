#pragma once

#include "log/logger.h"
#include "SPIFFS.h"

/// @brief Initializes the SPIFFS file system
/// @return Whether or not initialization failed
bool spiffs_init();

/// @brief Reads a file (`name`) from the SPIFFS file system and returns it as a char*; This does
/// need to be freed after use
/// @param name
/// @return A cstring representing all of the data inside of the file
const char* spiffs_read(const char* name = NULL, bool terminate = true);