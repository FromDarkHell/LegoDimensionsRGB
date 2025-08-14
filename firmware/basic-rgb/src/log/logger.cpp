#include "log/logger.h"

HardwareSerial LogUART = HardwareSerial(0);
Logger logger(&LogUART);

Logger::Logger(HardwareSerial* uart) {
    this->uart = uart;
}

void Logger::begin() {
    Serial.begin(BAUD_RATE);
    Serial.setDebugOutput(true);
    if (uart != nullptr) {
        uart->begin(BAUD_RATE, SERIAL_8N1, SOC_RX0, SOC_TX0);
    }
}

void Logger::log(const char* level, const char* fmt, va_list args) {
    char buffer[256];

    // Timestamp in ms since boot
    unsigned long now = millis();
    int n = snprintf(buffer, sizeof(buffer), "[%s][%lu ms]", level, now);
    vsnprintf(buffer + n, sizeof(buffer) - n, fmt, args);
    strncat(buffer, "\n", sizeof(buffer) - strlen(buffer) - 1);

    sendLogOutput(buffer);
    appendToLogBuffer(buffer);
}

void Logger::appendToLogBuffer(const char* msg) {
    size_t msgLen = strlen(msg);
    if (msgLen >= LOG_BUFFER_SIZE) {
        msg += (msgLen - LOG_BUFFER_SIZE + 1);
        msgLen = strlen(msg);
    }
    if (logBufferPos + msgLen >= LOG_BUFFER_SIZE) {
        size_t overflow = (logBufferPos + msgLen) - LOG_BUFFER_SIZE + 1;
        memmove(logBuffer, logBuffer + overflow, logBufferPos - overflow);
        logBufferPos -= overflow;
    }
    memcpy(logBuffer + logBufferPos, msg, msgLen);
    logBufferPos += msgLen;
    logBuffer[logBufferPos] = '\0';
}

void Logger::sendLogOutput(const char* msg) {
    Serial.print(msg);
    if (uart != nullptr) {
        uart->print(msg);
    }
}

void log_init() {
    logger.begin();
}

const char* log_get() {
    return logger.getBuffer();
}

void log_dbg(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger.log("DEBUG", fmt, args);
    va_end(args);
}

void log_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger.log("WARN", fmt, args);
    va_end(args);
}

void log_err(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger.log("ERROR", fmt, args);
    va_end(args);
}