
#include "web/captive_portal.h"

CaptivePortal::CaptivePortal(const char* ssid, const char* password) {
    this->ssid = ssid;
    this->password = password;
    this->html = spiffs_read("/portal.html");
    this->css = spiffs_read("/style.css");

    this->initialize();
}

CaptivePortal::~CaptivePortal() {
    this->dnsServer->stop();
    this->server->end();

    free(this->dnsServer);
    free(this->server);
}

void CaptivePortal::initialize() {
    this->start_soft_ap();
    this->start_dns();
    this->start_web();
}

void CaptivePortal::loop() {
    if (dnsServer != nullptr) {
        dnsServer->processNextRequest();
        delay(30);
    }

    ElegantOTA.loop();
}

void CaptivePortal::start_soft_ap() {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(this->accessPointIP, this->accessPointIP, this->subnetMask);
    WiFi.softAP(this->ssid, this->password, 6, false, 8, false);

    ESP_ERR_CHECK("CaptivePortal::start_soft_ap", "esp_wifi_set_ps", esp_wifi_set_ps(WIFI_PS_NONE));

    log_dbg("[CaptivePortal::start_soft_ap] Access point started (SSID: %s)", this->ssid);
}

void CaptivePortal::start_dns() {
    dnsServer = new DNSServer();

    // Set the TTL for DNS response and start the DNS server
    dnsServer->setTTL(3600);
    dnsServer->start(53, "*", this->accessPointIP);
}

void CaptivePortal::start_web() {
    server = new AsyncWebServer(80);
    // Required
    server->on("/connecttest.txt", [](AsyncWebServerRequest* request) {
        request->redirect("http://logout.net");
    });  // windows 11 captive portal workaround
    server->on("/wpad.dat", [](AsyncWebServerRequest* request) {
        request->send(404);
    });  // Honestly don't understand what this is but a 404 stops win 10 keep calling this
         // repeatedly and panicking the esp32 :)

    // Background responses: Probably not all are Required, but some are. Others might speed things
    // up? A Tier (commonly used by modern systems)
    server->on("/generate_204", [this](AsyncWebServerRequest* request) {
        request->redirect(this->localIPURL);
    });  // android captive portal redirect
    server->on("/redirect", [this](AsyncWebServerRequest* request) {
        request->redirect(this->localIPURL);
    });  // microsoft redirect
    server->on("/hotspot-detect.html", [this](AsyncWebServerRequest* request) {
        request->redirect(this->localIPURL);
    });  // apple call home
    server->on("/canonical.html", [this](AsyncWebServerRequest* request) {
        request->redirect(this->localIPURL);
    });  // firefox captive portal call home
    server->on("/success.txt", [this](AsyncWebServerRequest* request) {
        request->send(200);
    });  // firefox captive portal call home
    server->on("/ncsi.txt", [this](AsyncWebServerRequest* request) {
        request->redirect(this->localIPURL);
    });  // windows call home

    // Return 404 to webpage icon
    server->on("/favicon.ico", [](AsyncWebServerRequest* request) { request->send(404); });

    // Serve Basic HTML Page
    server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/css", this->css);
    });
    server->on("/", HTTP_ANY, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/html", this->html);
    });

    // Used for storing / saving config data
    server->on("/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
        const AsyncWebParameter* ssidParam = nullptr;
        const AsyncWebParameter* passwordParam = nullptr;

        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* param = request->getParam(i);

            if (param->name() == "ssid") {
                ssidParam = param;
            } else if (param->name() == "password") {
                passwordParam = param;
            }
        }

        if (ssidParam != nullptr) {
            config_set_string("ssid", ssidParam->value().c_str());
            const char* wifi_password = "\0";
            if (passwordParam != nullptr && !passwordParam->value().isEmpty()) {
                wifi_password = passwordParam->value().c_str();
            }

            config_set_string("password", wifi_password);
            config_set_bool("initialized", true);

            this->credentialsSaved = true;

            request->send(200, "text/html", "Complete! Plese wait!");
        } else {
            request->redirect("/");
        }
    });

    // the catch all
    server->onNotFound(
        [this](AsyncWebServerRequest* request) { request->redirect(this->localIPURL); });

    server->begin();

    ElegantOTA.begin(server);
}