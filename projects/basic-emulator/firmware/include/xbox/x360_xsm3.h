#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

// -----------------------------------------------------------------------
//  Interface 3 is an implementation of XSM3 security (class=0xFF / subclass=0xFD / protocol=0x13)
//  Descriptor: 9 (iface) + 6 (XSM3 inline vendor desc) = 15 bytes
//  No data endpoints.  Control requests are handled by X360XSM3 below.
// -----------------------------------------------------------------------
class X360IfaceXSM3 : public Adafruit_USBD_Interface {
   public:
    void setStringIndex(uint8_t idx) { _strid = idx; }
    uint16_t getInterfaceDescriptor(uint8_t itfnum_deprecated,
                                    uint8_t* buf,
                                    uint16_t bufsize) override;
};

// -----------------------------------------------------------------------
//  XSM3 authentication control-request handling.
//
//  The Xbox 360 drives the XSM3 authentication handshake entirely through
//  vendor control transfers targeting Interface 3 (bRequest 0x81/0x82/0x83/
//  0x86/0x87). The actual cryptography is delegated to libxsm3
//  (src/xbox/libxsm3); this wires TinyUSB's control transfer stages to it
//  and holds the small amount of state that must persist for the lifetime
//  of the USB connection, since the Xbox re-runs the handshake repeatedly
//  (e.g. on every power cycle) without the microcontroller ever detaching.
// -----------------------------------------------------------------------
namespace X360XSM3 {

// Called from tud_vendor_control_xfer_cb() at CONTROL_STAGE_SETUP.
// Returns true if this was an XSM3 request and has been handled (either
// queued via tud_control_xfer(), or ACK'd); false if it isn't one of ours
// and the caller should keep looking at other request handlers.
bool handleSetup(uint8_t rhport, tusb_control_request_t const* request);

// Called from tud_vendor_control_xfer_cb() at CONTROL_STAGE_ACK, once any
// OUT data phase queued by handleSetup() has landed in our buffers.
void handleAck(tusb_control_request_t const* request);

// Resets persisted XSM3 state. Only call this on a genuine physical
// disconnect (tud_umount_cb) - never on a re-authentication attempt, or
// the Xbox will stop trusting this device after its next power cycle.
void reset();

}  // namespace X360XSM3
