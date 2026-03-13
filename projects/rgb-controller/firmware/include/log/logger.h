#pragma once

#include <Arduino.h>

#define LOG_BUFFER_SIZE 4096  // Total log buffer size in bytes
#define UART_TX_PIN 17
#define UART_RX_PIN 16
#define BAUD_RATE 9600

class Logger {
   public:
    Logger(HardwareSerial* uartRef);

    void begin();
    void log(const char*, const char*, va_list args);
    const char* getBuffer() const { return logBuffer; }

   private:
    HardwareSerial* uart;
    char logBuffer[LOG_BUFFER_SIZE];
    size_t logBufferPos;

    void appendToLogBuffer(const char* msg);

    void sendLogOutput(const char* msg);
};

extern HardwareSerial LogUART;
extern Logger logger;

void log_init();
const char* log_get();
void log_dbg(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_err(const char* fmt, ...);

#define SOFT_ESP_ERR_LOG(tag, func, err)                                           \
    do {                                                                           \
        esp_err_t err_rc_ = (err);                                                 \
        if (unlikely(err_rc_ != ESP_OK)) {                                         \
            log_err("[%s] %s == %s (0x%X)", tag, func, esp_err_to_name(err), err); \
        }                                                                          \
    } while (0);

#define SOFT_ESP_ERR_CHECK(tag, func, err)                                         \
    do {                                                                           \
        esp_err_t err_rc_ = (err);                                                 \
        if (unlikely(err_rc_ != ESP_OK)) {                                         \
            log_err("[%s] %s == %s (0x%X)", tag, func, esp_err_to_name(err), err); \
            return false;                                                          \
        }                                                                          \
    } while (0);

#define ESP_ERR_CHECK(tag, func, err)                                              \
    do {                                                                           \
        esp_err_t err_rc_ = (err);                                                 \
        if (unlikely(err_rc_ != ESP_OK)) {                                         \
            log_err("[%s] %s == %s (0x%X)", tag, func, esp_err_to_name(err), err); \
            return;                                                                \
        }                                                                          \
    } while (0);
