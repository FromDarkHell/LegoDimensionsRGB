#include "playpad/playpad.h"

const TickType_t HOST_EVENT_TIMEOUT = 1;
const TickType_t CLIENT_EVENT_TIMEOUT = 1;

usb_host_client_handle_t client_handle = nullptr;
usb_device_handle_t device_handle = nullptr;

usb_device_handle_t playpad_handle = nullptr;
bool playpad_initialized = false;

static void transfer_cb(usb_transfer_t* transfer) {
    // This is function is called from within usb_host_client_handle_events().
    // Don't block and try to keep it short
    log_dbg("[transfer_cb] Status %d - Transferred 0x%d", transfer->status,
            transfer->actual_num_bytes);

    if (!playpad_initialized) {
        playpad_initialized = true;
    }

    // Free up our USB Transfer object
    // Theoretically, we could/should do this by avoiding deallocation entirely
    // But, for some reason, I would get errors when I tried to use a global for the transfer
    // *shrug*
    esp_err_t err = usb_host_transfer_free(transfer);
    ESP_ERR_CHECK("send_usb_packet", "usb_host_transfer_free", err);
}

void _send_usb_packet(usb_device_handle_t playpad,
                      int endpoint,
                      const uint8_t* packet,
                      const int packet_len) {
    assert(packet_len <= PLAYPAD_MAX_PACKET_SIZE);

    usb_transfer_t* transfer;

    esp_err_t err = usb_host_transfer_alloc(packet_len, 0, &transfer);
    ESP_ERR_CHECK("send_usb_packet", "usb_host_transfer_alloc", err);

    memcpy(transfer->data_buffer, packet, packet_len);
    transfer->num_bytes = packet_len;
    transfer->device_handle = playpad;
    transfer->bEndpointAddress = endpoint;

    transfer->callback = transfer_cb;
    transfer->context = NULL;

    err = usb_host_transfer_submit(transfer);
    ESP_ERR_CHECK("send_usb_packet", "usb_host_transfer_submit", err);
}

void _playpad_startup() {
    assert(playpad_handle != nullptr);

    delay(500);

    esp_err_t err = usb_host_interface_claim(client_handle, playpad_handle, PLAYPAD_INTERFACE,
                                             PLAYPAD_INTERFACE);
    ESP_ERR_CHECK("playpad_startup", "usb_host_interface_claim", err);

    delay(PLAYPAD_DELAY);

    _send_usb_packet(playpad_handle, PLAYPAD_WRITE_ENDPOINT, STARTUP_PACKET, STARTUP_PACKET_SIZE);
    initialize_pad_colors();
}

void _usb_client_event_callback(const usb_host_client_event_msg_t* event_msg, void* arg) {
    esp_err_t err;
    switch (event_msg->event) {
        /**< A new device has been enumerated and added to the USB Host Library */
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            log_dbg("[usb_client_event_callback] New Device Address: %d",
                    event_msg->new_dev.address);
            err = usb_host_device_open(client_handle, event_msg->new_dev.address, &device_handle);
            ESP_ERR_CHECK("usb_client_event_callback", "usb_host_device_open", err)

            usb_device_info_t dev_info;
            err = usb_host_device_info(device_handle, &dev_info);
            ESP_ERR_CHECK("usb_client_event_callback", "usb_host_device_info", err)

            const usb_device_desc_t* dev_desc;
            err = usb_host_get_device_descriptor(device_handle, &dev_desc);
            ESP_ERR_CHECK("usb_client_event_callback", "usb_host_get_device_descriptor", err)

            const usb_config_desc_t* config_desc;
            err = usb_host_get_active_config_descriptor(device_handle, &config_desc);
            ESP_ERR_CHECK("usb_client_event_callback", "usb_host_get_active_config_descriptor", err)

            log_dbg("[usb_client_event_callback] idVendor: 0x%X - idProduct: 0x%X",
                    dev_desc->idVendor, dev_desc->idProduct);

            if (dev_desc->idVendor == PLAYPAD_VENDOR_ID
                && dev_desc->idProduct == PLAYPAD_PRODUCT_ID) {
                log_dbg("[usb_client_event_callback] USB Playpad detected...");
                playpad_handle = device_handle;
                _playpad_startup();
            }

            break;
        /**< A device opened by the client is now gone */
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            log_dbg("[usb_client_event_callback] Device gone - Handle: %x",
                    event_msg->dev_gone.dev_hdl);

            if (event_msg->dev_gone.dev_hdl == playpad_handle) {
                // err = usb_host_interface_release(client_handle, playpad_handle,
                // PLAYPAD_INTERFACE); ESP_ERR_CHECK("usb_client_event_callback",
                // "usb_host_interface_release", err);

                // err = usb_host_device_close(client_handle, playpad_handle);
                // ESP_ERR_CHECK("usb_client_event_callback", "usb_host_device_close", err);

                playpad_handle = nullptr;
                playpad_initialized = false;
            }

            break;
        default:
            log_dbg("[usb_client_event_callback] Unknown USB Event %d", event_msg->event);
            break;
    }
}

bool playpad_init() {
    log_dbg("[playpad_init] Initializing USB Host...");

    const usb_host_config_t config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t err = usb_host_install(&config);
    SOFT_ESP_ERR_CHECK("playpad_init", "usb_host_install", err);

    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {.client_event_callback = _usb_client_event_callback,
                  .callback_arg = client_handle}};

    err = usb_host_client_register(&client_config, &client_handle);
    SOFT_ESP_ERR_CHECK("playpad_init", "usb_host_client_register", err);

    log_dbg("[playpad_init] USB Host Initialized...");

    playpad_loop();

    return true;
}

bool playpad_loop() {
    uint32_t event_flags;

    esp_err_t err = usb_host_lib_handle_events(HOST_EVENT_TIMEOUT, &event_flags);

    if (err == ESP_OK) {
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            log_warn("[playpad_loop] No more USB clients...");
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            log_warn("[playpad_loop] No more USB devices...");
        }
    } else if (err != ESP_ERR_TIMEOUT) {
        SOFT_ESP_ERR_LOG("playpad_loop", "usb_host_lib_handle_events", err)
    }

    err = usb_host_client_handle_events(client_handle, CLIENT_EVENT_TIMEOUT);
    if (err != ESP_ERR_TIMEOUT) {
        SOFT_ESP_ERR_CHECK("playpad_loop", "usb_host_client_handle_events", err)
    }

    return true;
}

/// ==== Playpad Interaction / Commands ====

esp_err_t initialize_pad_colors() {
    playpad_color left_color = playpad_color::from_uint(config_get_uint("left_pad", 0xFF0000));
    playpad_color middle_color = playpad_color::from_uint(config_get_uint("middle_pad", 0x00FF00));
    playpad_color right_color = playpad_color::from_uint(config_get_uint("right_pad", 0x0000FF));

    return change_pad_colors(left_color, middle_color, right_color);
}

esp_err_t change_pad_colors(playpad_color left, playpad_color middle, playpad_color right) {
    bool all_matching = (left == middle && middle == right);
    if (all_matching) {
        return change_pad_color(PLAYPAD_PAD_ALL, middle);
    }

    playpad_color colors[3] = {middle, left, right};
    for (int i = 0; i < 3; i++) {
        playpad_color color = colors[i];
        change_pad_color(i + 1, color);
        delay(PLAYPAD_DELAY);
    }

    return ESP_OK;
}

esp_err_t change_pad_color(playpad_pad pad, playpad_color color) {
    uint8_t command[] = {0x55, 0x06, 0xC0, 0x02, pad, color.red, color.green, color.blue};
    uint8_t packet[32];

    // TODO: Add basic error checking for this
    construct_packet(command, sizeof(command), packet);

    _send_usb_packet(playpad_handle, PLAYPAD_WRITE_ENDPOINT, packet, PLAYPAD_MAX_PACKET_SIZE);

    return ESP_OK;
}