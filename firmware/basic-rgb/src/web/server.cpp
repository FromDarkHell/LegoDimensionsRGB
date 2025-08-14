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
    if (!success) {
        return;
    }

    this->start_web();
}

bool LegoServer::start_mdns() {
    if (!MDNS.begin("playpad")) {
        log_dbg("[server::start_mdns] Error setting up MDNS!");

        return false;
    }

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

    log_dbg("[server::connect_to_ap] Connected to '%s' (IP Address: %s)", this->ssid,
            WiFi.localIP().toString());

    return true;
}

void LegoServer::start_web() {
    server = new AsyncWebServer(80);

    server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/css", this->css);
    });

    server->on("/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        config_clear();
        request->send(200, "text/html", "Success! Please wait");
        esp_restart();
    });

    server->on("/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/html", "Success! Please wait");
        esp_restart();
    });

    server->on("/change", HTTP_POST, [this](AsyncWebServerRequest* request) {
        const AsyncWebParameter* left = nullptr;
        const AsyncWebParameter* middle = nullptr;
        const AsyncWebParameter* right = nullptr;

        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* param = request->getParam(i);

            if (param->name() == "left") {
                left = param;
            } else if (param->name() == "middle") {
                middle = param;
            } else if (param->name() == "right") {
                right = param;
            }
        }

        playpad_color left_color = playpad_color::from_hex(left->value().c_str());
        playpad_color middle_color = playpad_color::from_hex(middle->value().c_str());
        playpad_color right_color = playpad_color::from_hex(right->value().c_str());

        change_pad_colors(left_color, middle_color, right_color);

        config_set_uint("left_pad", left_color.to_uint());
        config_set_uint("middle_pad", middle_color.to_uint());
        config_set_uint("right_pad", right_color.to_uint());

        request->redirect("/");
    });

    server->on("/logs", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", log_get());
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
