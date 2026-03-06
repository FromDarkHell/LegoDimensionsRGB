#include "web/server.h"

LegoServer::LegoServer(const char *ssid, const char *password, PlayPad *playpadInstance)
{
    this->ssid = ssid;
    this->password = password;
    this->html = fs_read("/index.html");
    this->css = fs_read("/style.css");
    this->playpad = playpadInstance;

    this->initialize();
}
LegoServer::~LegoServer() {}

void LegoServer::initialize()
{
    success = true;

    success &= this->connect_to_ap();
    if (!success)
    {
        return;
    }

    success &= this->start_mdns();
    if (!success)
    {
        return;
    }

    this->start_web();

    this->load_toybox();
}

bool LegoServer::start_mdns()
{
    if (!MDNS.begin("emupad"))
    {
        log_dbg("[server::start_mdns] Error setting up MDNS!");

        return false;
    }

    return true;
}

bool LegoServer::connect_to_ap()
{
    WiFi.mode(WIFI_STA);

    log_dbg("[server::connect_to_ap] Connecting to '%s'", this->ssid);

    int attempts = 0;
    while (attempts < 5)
    {
        wl_status_t status = (wl_status_t)WiFi.begin(this->ssid, this->password);

        while (status != WL_CONNECTED)
        {
            if (status == WL_CONNECT_FAILED)
                break;

            status = (wl_status_t)WiFi.status();
            delay(500);
        }

        if (status == WL_CONNECTED)
        {
            break;
        }

        attempts++;
        log_dbg("[server::connect_to_ap] Attempt %d to connect to '%s' failed", attempts, this->ssid);
        delay(1000);
    }

    log_dbg("[server::connect_to_ap] Connected to '%s' (IP Address: %s)", this->ssid,
            WiFi.localIP().toString().c_str());

    return true;
}

void LegoServer::start_web()
{
    server = new AsyncWebServer(80);

    server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest *request)
               { request->send(200, "text/css", this->css); });

    server->on("/reset", HTTP_POST, [this](AsyncWebServerRequest *request)
               {
                   config.clear();
                   request->send(200, "text/html", "Success! Please wait");
                   rp2040.reboot(); });

    server->on("/restart", HTTP_POST, [this](AsyncWebServerRequest *request)
               {
                   request->send(200, "text/html", "Success! Please wait");
                   rp2040.reboot(); });

    server->on("/logs", HTTP_GET, [this](AsyncWebServerRequest *request)
               { request->send(200, "text/plain", log_get()); });

    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { request->send(200, "text/html", this->html); });

    server->on(
        "/html", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            static String body;

            if (index == 0)
            {
                body = "";
                body.reserve(total);
            }

            body += String((char *)data).substring(0, len);

            if (index + len == total)
            {
                fs_write("/index.html", body.c_str());
                free((void *)this->html);
                this->html = strdup(body.c_str());
                request->send(200, "text/plain", "OK");
            }
        });

    server->onNotFound([this](AsyncWebServerRequest *request)
                       {
        String path = request->url();
        // log_dbg("[server] Request for %s", path.c_str());

        if (fs_exists(path.c_str())) {
            String contentType = "";
            if (path.endsWith(".webp")) {
                contentType = "image/webp";
            }

            AsyncWebServerResponse* response = request->beginResponse(LittleFS, path, contentType);

            // Cache static assets for 7 days and mark them as immutable, so that browsers can cache them aggressively
            response->addHeader("Cache-Control", "public, max-age=604800, immutable");

            request->send(response);
        } else {
            request->send(404, "text/html", "Failed to find file");
        } });

    this->add_lego_endpoints();

    server->begin();

    ElegantOTA.begin(server);
}

void LegoServer::loop()
{
    MDNS.update();
    ElegantOTA.loop();
}

bool LegoServer::add_lego_endpoints()
{
    server->on("/toybox", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
        if (this->toybox == nullptr) {
            this->load_toybox();
        }
        request->send(200, "application/json", this->toybox->serialize()); });

    // Running a DELETE request onto the `/toybox` endpoint, adds a new tag to the toybox
    server->on("/toybox", HTTP_DELETE, [this](AsyncWebServerRequest *request)
               {
        if (this->toybox == nullptr) {
            this->load_toybox();
        }

        const char* tag_uid = nullptr;
        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* param = request->getParam(i);

            if (param->name() == "uid") {
                tag_uid = param->value().c_str();
            } else {
                log_warn("[server::toybox] Unknown param %s", param->name().c_str());
            }
        }

        if (tag_uid == nullptr) {
            request->send(500);
        }

        this->toybox->removeToy(tag_uid);
        this->store_toybox();
        request->send(200, "application/json", this->toybox->serialize()); });

    // Running a PUT request onto the `/toybox` endpoint, adds a new tag to the toybox
    server->on("/toybox", HTTP_PUT, [this](AsyncWebServerRequest *request)
               {
        if (this->toybox == nullptr) {
            this->load_toybox();
        }

        const char* tag_name = nullptr;
        int tag_id = 0xFFFF;
        const char* tag_type = nullptr;

        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* param = request->getParam(i);

            if (param->name() == "name") {
                tag_name = param->value().c_str();
            } else if (param->name() == "id") {
                tag_id = atoi(param->value().c_str());
            } else if (param->name() == "type") {
                tag_type = param->value().c_str();
            } else {
                log_warn("[server::toybox] Unknown param %s", param->name().c_str());
            }
        }

        log_dbg("[server::toybox] Adding new tag to toybox / Name: %s / ID: %d / Type: %s",
                tag_name, tag_id, tag_type);

        this->toybox->addToy(tag_name, tag_id, tag_type);
        this->store_toybox();
        request->send(200, "application/json", this->toybox->serialize()); });

    // *Playpad* Management

    server->on("/playpad", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
                   if (this->playpad == nullptr)
                   {
                       request->send(404);
                       return;
                   }

                   JsonDocument playpadState;

                    static constexpr struct {
                        const char *key;
                        PadLocation location;
                    } PAD_ENTRIES[] = {
                        { "left",   PadLocation::Left   },
                        { "center", PadLocation::Center },
                        { "right",  PadLocation::Right  },
                    };


                    for (const auto &entry : PAD_ENTRIES)
                    {
                        JsonObject obj = playpadState[entry.key].to<JsonObject>();
                        this->playpad->getPad(entry.location)->toJson(obj);
                    }

                   String result;
                   serializeJson(playpadState, result);
                   request->send(200, "application/json", result); });

    // Running a PUT request onto the `/playpad` endpoint
    // It lets you move a tag around based on the UID.
    server->on("/playpad", HTTP_PUT, [this](AsyncWebServerRequest *request)
               {
        if (this->toybox == nullptr) {
            this->load_toybox();
        }

        const char* tag_uid;
        const char* tag_index = "-1";
        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* param = request->getParam(i);

            if (param->name() == "uid") {
                tag_uid = param->value().c_str();
            } else if (param->name() == "index") {
                tag_index = param->value().c_str();
            } else {
                log_warn("[server::toybox] Unknown param %s", param->name().c_str());
            }
        }

        JsonDocument* tag = this->toybox->getByUID(tag_uid);
        if (tag == nullptr) {
            request->send(404);
            return;
        }

        (*tag)["index"] = tag_index;

        this->store_toybox();
        request->send(200, "application/json", this->toybox->serialize()); });

    return true;
}

bool LegoServer::load_toybox()
{
    this->toybox = new Toybox();

    if (!fs_exists("/toybox.json"))
    {
        return true;
    }

    const char *json = fs_read("/toybox.json");
    return this->toybox->deserialize(json);
}

bool LegoServer::store_toybox()
{
    if (this->toybox == nullptr)
    {
        return false;
    }

    String serialized = this->toybox->serialize();
    const char *content = serialized.c_str();
    bool success = fs_write("/toybox.json", content);
    if (!success)
    {
        log_warn("[LegoServer::store_toybox] Failed to write toybox cache file");
    }

    return success;
}