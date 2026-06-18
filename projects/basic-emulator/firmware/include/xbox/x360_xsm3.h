#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

// -----------------------------------------------------------------------
//  Interface 3 is an implementation of XSM3 security (class=0xFF / subclass=0xFD / protocol=0x13)
//  Descriptor: 9 (iface) + 6 (XSM3 inline vendor desc) = 15 bytes
//  No data endpoints.  Control requests are ACK'd in x360_usb.cpp.
// -----------------------------------------------------------------------
class X360IfaceXSM3 : public Adafruit_USBD_Interface {
   public:
    void setStringIndex(uint8_t idx) { _strid = idx; }
    uint16_t getInterfaceDescriptor(uint8_t itfnum_deprecated,
                                    uint8_t* buf,
                                    uint16_t bufsize) override;
};
