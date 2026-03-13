#include "web/captive_portal.h"
#include "web/server.h"

#include "config/config.h"
#include "fs/fs.h"
#include "log/logger.h"
#include "playpad/playpad.h"

CaptivePortal* initializationPortal = nullptr;
LegoServer* webServer = nullptr;

void create_captive_portal() {
    log_dbg("[create_captive_portal] Creating captive portal...");
    initializationPortal = new CaptivePortal("captive", NULL);
    log_dbg("[create_captive_portal] Waiting for initialization...");
}

void setup() {
    log_init();
    config_init();
    spiffs_init();

    if (!config_get_bool("initialized", false)) {
        create_captive_portal();
    }
}

void loop() {
    // If we're waiting to get an SSID, don't bother initializing the USB stuff, it could slow
    // things down The captive portal stuff felt super finnicky to me anyways...
    if (initializationPortal != nullptr) {
        // For the sake of seeing if the portal is initialized, just blink the LED on/off while we
        // check for DNS requests/etc.
        initializationPortal->loop();

        // Check if the credentials have been saved, if so, it's time to initialize our connection
        // to the WiFi instance.
        if (initializationPortal->credentialsSaved) {
            delete initializationPortal;
            initializationPortal = nullptr;
        }

        return;
    }

    if (webServer == nullptr) {
        log_dbg("[loop] Initializing Playpad Server...");
        webServer =
            new LegoServer(config_get_string("ssid", NULL), config_get_string("password", NULL));
        if (!webServer->success) {
            delete webServer;
            webServer = nullptr;

            create_captive_portal();
            return;
        }

        assert(webServer->success);

        playpad_init();
    }

    if (webServer != nullptr) {
        webServer->loop();
        playpad_loop();
    }
}