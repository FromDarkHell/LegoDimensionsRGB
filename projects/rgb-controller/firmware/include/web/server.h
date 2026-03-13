#pragma once

#include "web/web.h"

#include <Arduino.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include <ElegantOTA.h>
#include <esp_wifi.h>
#include <ESPmDNS.h>
#include "config/config.h"
#include "fs/fs.h"
#include "log/logger.h"
#include "playpad/playpad.h"

class LegoServer {
   public:
    LegoServer(const char*, const char*);
    ~LegoServer();

    void initialize();
    void loop();

    bool success = false;

   protected:
    bool connect_to_ap();
    bool start_mdns();
    void start_web();

    const char* ssid;
    const char* password;

    const char* html = nullptr;
    const char* css = nullptr;

    AsyncWebServer* server = nullptr;
};