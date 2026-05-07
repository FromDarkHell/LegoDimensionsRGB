#include "web/server.h"

LegoServer::LegoServer(const char* ssid, const char* password) {
    this->ssid = ssid;
    this->password = password;
    this->html = spiffs_read("/index.html");
    this->css = spiffs_read("/style.css");

    this->initialize();
}

LegoServer::~LegoServer() {}

void LegoServer::initialize() {
    success = true;

    success &= this->connect_to_ap();
    if (!success) {
        return;
    }

    success &= this->start_mdns();

    this->start_web();
}

bool LegoServer::start_mdns() {
    // Allow a user-configured name, otherwise derive one from the MAC address
    // so every device is guaranteed a unique hostname with zero network probing.
    const char* stored = config_get_string("mdns_name", "");

    char hostname[32];
    if (strlen(stored) > 0) {
        strlcpy(hostname, stored, sizeof(hostname));
    } else {
        // Use the last 3 octets of the MAC address just to avoid duplicates
        uint8_t mac[6];
        WiFi.macAddress(mac);
        snprintf(hostname, sizeof(hostname), "playpad-%02x%02x%02x", mac[3], mac[4], mac[5]);
    }

    free((void*)stored);

    if (!MDNS.begin(hostname)) {
        log_err("[server::start_mdns] MDNS.begin('%s') failed", hostname);
        return false;
    }

    log_dbg("[server::start_mdns] Registered mDNS hostname: %s.local", hostname);
    return true;
}

bool LegoServer::connect_to_ap() {
    log_dbg("[server::connect_to_ap] Connecting to '%s'", this->ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(this->ssid, this->password);

    wl_status_t status = WiFi.status();
    while (status != WL_CONNECTED) {
        if (status == WL_CONNECT_FAILED) {
            return false;
        }

        status = WiFi.status();
        delay(500);
    }

    log_dbg("[server::connect_to_ap] Connected to '%s' (IP: %s)", this->ssid,
            WiFi.localIP().toString().c_str());
    return true;
}

void LegoServer::start_web() {
    server = new AsyncWebServer(80);

    server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/css", this->css);
    });

    server->on("/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        config_clear();
        request->send(200, "text/html", "Success! Please wait");
        esp_restart();
    });

    server->on("/restart", HTTP_POST, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", "Success! Please wait");
        esp_restart();
    });

    server->on("/logs", HTTP_GET,
               [](AsyncWebServerRequest* request) { request->send(200, "text/plain", log_get()); });

    // ----------------------------------------------------------------
    //  /change  — update pad colors and persist to config
    // ----------------------------------------------------------------
    server->on("/change", HTTP_POST, [](AsyncWebServerRequest* request) {
        const AsyncWebParameter* leftParam = nullptr;
        const AsyncWebParameter* middleParam = nullptr;
        const AsyncWebParameter* rightParam = nullptr;

        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* p = request->getParam(i);
            if (p->name() == "left") {
                leftParam = p;
            } else if (p->name() == "middle") {
                middleParam = p;
            } else if (p->name() == "right") {
                rightParam = p;
            }
        }

        if (!leftParam || !middleParam || !rightParam) {
            request->send(400, "text/plain", "Missing color parameter(s)");
            return;
        }

        const PadColor left = PadColor::fromHex(leftParam->value().c_str());
        const PadColor center = PadColor::fromHex(middleParam->value().c_str());
        const PadColor right = PadColor::fromHex(rightParam->value().c_str());

        // Apply immediately; Gateway decides whether to use COL or COLAL
        if (left == center && center == right) {
            toypad.switchPad(PadLocation::ALL, center);
        } else {
            toypad.switchPads(center, left, right);
        }

        // Persist so initColors() restores them on next boot/reconnect
        config_set_uint("left_pad", left.toUint32());
        config_set_uint("middle_pad", center.toUint32());
        config_set_uint("right_pad", right.toUint32());

        request->redirect("/");
    });

    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/html", this->html);
    });

    server->begin();
    ElegantOTA.begin(server);
}

void LegoServer::loop() {
    ElegantOTA.loop();
}