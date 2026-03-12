#pragma once

#include <Arduino.h>

#define LOG_BUFFER_SIZE 4096
#define BAUD_RATE 9600

#define LOG_UART Serial1
#define LOG_UART_TX 17
#define LOG_UART_RX 16

class Logger {
   public:
    void begin();
    void log(const char* level, const char* fmt, va_list args);

    void blinkStatus(int count, int interval_ms);

    const char* getBuffer() const { return logBuffer; }

   private:
    char logBuffer[LOG_BUFFER_SIZE];
    size_t logBufferPos = 0;

    void appendToLogBuffer(const char* msg);
    void sendLogOutput(const char* msg);
};

extern Logger logger;

void log_init();
const char* log_get();
void log_dbg(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_err(const char* fmt, ...);