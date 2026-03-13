#include "fs/fs.h"

bool spiffs_init() {
    log_dbg("[spiffs_init] Initializing SPIFFS");

    return SPIFFS.begin(true);
}

const char* spiffs_read(const char* name, bool terminate) {
    File file = SPIFFS.open(name, "r");
    if (!file) {
        return NULL;
    }

    size_t size = file.size();
    char* buffer = new char[size + (terminate ? 1 : 0)];  // +1 for null terminator

    size_t read = file.readBytes(buffer, size);

    if (terminate) {
        buffer[read] = '\0';
    }

    file.close();

    return buffer;
}