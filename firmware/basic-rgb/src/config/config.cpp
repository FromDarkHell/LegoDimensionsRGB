#include "config/config.h"

Preferences _preferences;

void config_clear() {
    nvs_flash_erase();  // erase the NVS partition and...
    nvs_flash_init();   // initialize the NVS partition.
}

void config_init() {
    log_dbg("[config_init] Initializing preference");
    _preferences.begin("basic-rgb", false);
}

bool config_get_bool(const char* key, const bool default_value) {
    return _preferences.getBool(key, default_value);
}

bool config_set_bool(const char* key, const bool value) {
    _preferences.putBool(key, value);
    return true;
}

uint8_t config_get_uchar(const char* key, const uint8_t default_value) {
    return _preferences.getUChar(key, default_value);
}

bool config_set_uchar(const char* key, const uint8_t value) {
    _preferences.putUChar(key, value);
    return true;
}

uint32_t config_get_uint(const char* key, const uint32_t default_value) {
    return _preferences.getUInt(key, default_value);
}

bool config_set_uint(const char* key, const uint32_t value) {
    _preferences.putUInt(key, value);
    return true;
}

const char* config_get_string(const char* key, const char* default_value) {
    String result = _preferences.getString(key, String(default_value));

    char* buf = (char*)malloc(result.length() + 1);
    strncpy(buf, result.c_str(), result.length());
    buf[result.length()] = '\0';

    return buf;
}

bool config_set_string(const char* key, const char* value) {
    _preferences.putString(key, value);
    return true;
}