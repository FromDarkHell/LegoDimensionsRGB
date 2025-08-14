#pragma once

#include <nvs_flash.h>
#include <Preferences.h>
#include "log/logger.h"
#include "stdint.h"

void config_clear();
void config_init();

bool config_get_bool(const char*, const bool);
bool config_set_bool(const char*, const bool);

uint8_t config_get_uchar(const char*, const uint8_t);
bool config_set_uchar(const char*, const uint8_t);

uint32_t config_get_uint(const char*, const uint32_t);
bool config_set_uint(const char*, const uint32_t);

const char* config_get_string(const char*, const char*);
bool config_set_string(const char*, const char*);