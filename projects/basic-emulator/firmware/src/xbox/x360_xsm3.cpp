#include "xbox/x360_xsm3.h"

// -----------------------------------------------------------------------
//  Interface 3 -> XSM3 security stub
//
//  Allocates:  1 interface number, no endpoints.
//  _strid must be set by X360PortalUSB::begin() before addInterface().
//  Total descriptor bytes: 9 + 6 = 15
// -----------------------------------------------------------------------
uint16_t X360IfaceXSM3::getInterfaceDescriptor(uint8_t /*itfnum_deprecated*/,
                                               uint8_t* buf,
                                               uint16_t bufsize) {
    constexpr uint16_t LEN = 9 + 6;
    if (!buf) {
        return LEN;
    }
    if (bufsize < LEN) {
        return 0;
    }

    uint8_t const itfnum = TinyUSBDevice.allocInterface(1);

    uint8_t* p = buf;

    // Interface descriptor (no endpoints)
    *p++ = 0x09;
    *p++ = 0x04;
    *p++ = itfnum;
    *p++ = 0x00;
    *p++ = 0x00;  // bNumEndpoints
    *p++ = 0xFF;
    *p++ = 0xFD;
    *p++ = 0x13;
    *p++ = _strid;  // iInterface → XSM3 string

    // XSM3 inline vendor descriptor (type=0x41) -> verbatim from OEM capture.
    *p++ = 0x06;
    *p++ = 0x41;
    *p++ = 0x00;
    *p++ = 0x01;
    *p++ = 0x01;
    *p++ = 0x03;

    return LEN;
}
