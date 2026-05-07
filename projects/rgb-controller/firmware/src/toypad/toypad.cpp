#include "toypad/toypad.h"

Toypad* Toypad::_instance = nullptr;
Toypad toypad;

// ----------------------------------------------------------------
//  Lifecycle
// ----------------------------------------------------------------

bool Toypad::begin() {
    if (_instance != nullptr) {
        return true;
    }

    _instance = this;

    log_dbg("[Toypad::begin] Installing USB host...");

    const usb_host_config_t hostCfg = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    esp_err_t err = usb_host_install(&hostCfg);
    if (err != ESP_OK) {
        log_err("[Toypad::begin] usb_host_install: %s", esp_err_to_name(err));
        return false;
    }

    const usb_host_client_config_t clientCfg = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {.client_event_callback = _clientEventCallback, .callback_arg = this},
    };

    err = usb_host_client_register(&clientCfg, &_clientHandle);
    if (err != ESP_OK) {
        log_err("[Toypad::begin] usb_host_client_register: %s", esp_err_to_name(err));
        return false;
    }

    log_dbg("[Toypad::begin] USB host ready.");
    loop();  // initial event pump so the first device is seen immediately

    return true;
}

bool Toypad::loop() {
    uint32_t eventFlags;

    esp_err_t err = usb_host_lib_handle_events(/*timeout_ticks=*/1, &eventFlags);
    if (err == ESP_OK) {
        if (eventFlags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            log_warn("[Toypad::loop] No more USB clients");
        }
        if (eventFlags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            log_warn("[Toypad::loop] No more USB devices");
        }
    }

    err = usb_host_client_handle_events(_clientHandle, /*timeout_ticks=*/1);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        log_err("[Toypad::loop] usb_host_client_handle_events: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

// ----------------------------------------------------------------
//  Pad commands
// ----------------------------------------------------------------

void Toypad::switchPad(PadLocation pad, PadColor color) {
    uint8_t payload[] = {static_cast<uint8_t>(pad), color.r, color.g, color.b};
    _sendPacket(CommandPacket::build(ToypadCommand::COL, _nextCID(), this->_padPlatform, payload,
                                     sizeof(payload)));
}

void Toypad::switchPads(PadColor center, PadColor left, PadColor right) {
    // COLAL packs 3 × (location byte + RGB) = 12 bytes
    uint8_t payload[12] = {
        static_cast<uint8_t>(PadLocation::CENTER), center.r, center.g, center.b,
        static_cast<uint8_t>(PadLocation::LEFT),   left.r,   left.g,   left.b,
        static_cast<uint8_t>(PadLocation::RIGHT),  right.r,  right.g,  right.b,
    };
    _sendPacket(CommandPacket::build(ToypadCommand::COLAL, _nextCID(), this->_padPlatform, payload,
                                     sizeof(payload)));
}

void Toypad::flashPad(PadLocation pad,
                      uint8_t onTime,
                      uint8_t offTime,
                      uint8_t count,
                      PadColor offColor) {
    uint8_t payload[] = {
        static_cast<uint8_t>(pad), onTime, offTime, count, offColor.r, offColor.g, offColor.b,
    };
    _sendPacket(CommandPacket::build(ToypadCommand::FLASH, _nextCID(), this->_padPlatform, payload,
                                     sizeof(payload)));
}

void Toypad::flashPads(PadFlashConfig center, PadFlashConfig left, PadFlashConfig right) {
    // FLSAL: 3 × (enable + onTime + offTime + count + R + G + B) = 21 bytes
    const PadFlashConfig pads[3] = {center, left, right};
    uint8_t payload[21]{};
    for (int i = 0; i < 3; i++) {
        int o = i * 7;
        payload[o + 0] = pads[i].enabled ? 1 : 0;
        payload[o + 1] = pads[i].onTime;
        payload[o + 2] = pads[i].offTime;
        payload[o + 3] = pads[i].count;
        payload[o + 4] = pads[i].offColor.r;
        payload[o + 5] = pads[i].offColor.g;
        payload[o + 6] = pads[i].offColor.b;
    }
    _sendPacket(CommandPacket::build(ToypadCommand::FLSAL, _nextCID(), this->_padPlatform, payload,
                                     sizeof(payload)));
}

void Toypad::fadePad(PadLocation pad, uint8_t speed, uint8_t count, PadColor color) {
    uint8_t payload[] = {
        static_cast<uint8_t>(pad), speed, count, color.r, color.g, color.b,
    };
    _sendPacket(CommandPacket::build(ToypadCommand::FADE, _nextCID(), this->_padPlatform, payload,
                                     sizeof(payload)));
}

void Toypad::fadePads(PadFadeConfig center, PadFadeConfig left, PadFadeConfig right) {
    // FADAL: 3 × (enable + speed + count + R + G + B) = 18 bytes
    const PadFadeConfig pads[3] = {center, left, right};
    uint8_t payload[18]{};
    for (int i = 0; i < 3; i++) {
        int o = i * 6;
        payload[o + 0] = pads[i].enabled ? 1 : 0;
        payload[o + 1] = pads[i].speed;
        payload[o + 2] = pads[i].count;
        payload[o + 3] = pads[i].color.r;
        payload[o + 4] = pads[i].color.g;
        payload[o + 5] = pads[i].color.b;
    }
    _sendPacket(CommandPacket::build(ToypadCommand::FADAL, _nextCID(), this->_padPlatform, payload,
                                     sizeof(payload)));
}

void Toypad::initColors() {
    PadColor left = PadColor::fromUint32(config_get_uint("left_pad", 0xFF0000));
    PadColor center = PadColor::fromUint32(config_get_uint("middle_pad", 0x00FF00));
    PadColor right = PadColor::fromUint32(config_get_uint("right_pad", 0x0000FF));

    if (left == center && center == right) {
        switchPad(PadLocation::ALL, center);
    } else {
        switchPads(center, left, right);
    }
}

// ----------------------------------------------------------------
//  Internal helpers
// ----------------------------------------------------------------

void Toypad::_sendPacket(const CommandPacket& pkt) {
    if (!_deviceHandle) {
        log_warn("[Toypad] _sendPacket: no device connected");
        return;
    }

    usb_transfer_t* transfer;
    esp_err_t err = usb_host_transfer_alloc(PLAYPAD_MAX_PACKET, 0, &transfer);
    if (err != ESP_OK) {
        log_err("[Toypad] transfer_alloc: %s", esp_err_to_name(err));
        return;
    }

    memcpy(transfer->data_buffer, pkt.data, PLAYPAD_MAX_PACKET);
    transfer->num_bytes = PLAYPAD_MAX_PACKET;
    transfer->device_handle = _deviceHandle;
    transfer->bEndpointAddress = PLAYPAD_WRITE_EP;
    transfer->callback = _transferCallback;
    transfer->context = nullptr;

    err = usb_host_transfer_submit(transfer);
    if (err != ESP_OK) {
        log_err("[Toypad] transfer_submit: %s", esp_err_to_name(err));
        usb_host_transfer_free(transfer);
    }
}

void Toypad::_startup() {
    log_dbg("[Toypad::_startup] Claiming interface...");

    esp_err_t err = usb_host_interface_claim(_clientHandle, _deviceHandle, PLAYPAD_INTERFACE,
                                             PLAYPAD_INTERFACE);
    if (err != ESP_OK) {
        log_err("[Toypad::_startup] interface_claim: %s", esp_err_to_name(err));
        return;
    }

    // WAKE: send "(c) LEGO 2014"
    static const uint8_t WAKE_PAYLOAD[] = {
        0x28, 0x63, 0x29, 0x20, 0x4C, 0x45, 0x47, 0x4F, 0x20, 0x32, 0x30, 0x31, 0x34,
    };
    _sendPacket(CommandPacket::build(ToypadCommand::WAKE, _nextCID(), this->_padPlatform,
                                     WAKE_PAYLOAD, sizeof(WAKE_PAYLOAD)));

    initColors();
}

void Toypad::_onDeviceConnected(uint8_t address) {
    log_dbg("[Toypad] New USB device at address %d", address);

    esp_err_t err = usb_host_device_open(_clientHandle, address, &_deviceHandle);
    if (err != ESP_OK) {
        log_err("[Toypad] device_open: %s", esp_err_to_name(err));
        return;
    }

    const usb_device_desc_t* desc;
    err = usb_host_get_device_descriptor(_deviceHandle, &desc);
    if (err != ESP_OK) {
        log_err("[Toypad] get_device_descriptor: %s", esp_err_to_name(err));
        usb_host_device_close(_clientHandle, _deviceHandle);
        _deviceHandle = nullptr;
        return;
    }

    log_dbg("[Toypad] idVendor=0x%04X idProduct=0x%04X", desc->idVendor, desc->idProduct);

    ToypadPlatform platform = getToypadPlatformFromIDs(desc->idVendor, desc->idProduct);
    if (platform != ToypadPlatform::UNK) {
        this->_padPlatform = platform;
        log_dbg("[Toypad] Playpad detected (platform=0x%04X) - starting up", this->_padPlatform);
        _startup();
    } else {
        usb_host_device_close(_clientHandle, _deviceHandle);
        _deviceHandle = nullptr;
    }
}

void Toypad::_onDeviceDisconnected(usb_device_handle_t handle) {
    if (handle == _deviceHandle) {
        log_dbg("[Toypad] Playpad disconnected");
        usb_host_interface_release(_clientHandle, _deviceHandle, PLAYPAD_INTERFACE);
        usb_host_device_close(_clientHandle, _deviceHandle);
        _deviceHandle = nullptr;
    }
}

// ----------------------------------------------------------------
//  Static USB callbacks
// ----------------------------------------------------------------

void Toypad::_clientEventCallback(const usb_host_client_event_msg_t* msg, void* /*arg*/) {
    if (_instance == nullptr) {
        return;
    }
    switch (msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            _instance->_onDeviceConnected(msg->new_dev.address);
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            _instance->_onDeviceDisconnected(msg->dev_gone.dev_hdl);
            break;
        default:
            log_warn("[Toypad] Unknown USB client event: %d", msg->event);
            break;
    }
}

void Toypad::_transferCallback(usb_transfer_t* transfer) {
    log_dbg("[Toypad] Transfer done - status=%d bytes=%d", transfer->status,
            transfer->actual_num_bytes);
    usb_host_transfer_free(transfer);
}