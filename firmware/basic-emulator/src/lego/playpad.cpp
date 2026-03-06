#include "lego/playpad.h"
#include "log/logger.h"

PlayPad *PlayPad::_instance = nullptr;

PlayPad::PlayPad()
{
    _instance = this;
    this->_registerHandlers();
}

void PlayPad::begin()
{
    if (!TinyUSBDevice.isInitialized())
    {
        TinyUSBDevice.begin(0);
    }

    _configureDevice();

    _usb_hid.enableOutEndpoint(true);
    _usb_hid.setPollInterval(1);
    _usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
    _usb_hid.setStringDescriptor(PLAYPAD_PRODUCT);
    _usb_hid.setReportCallback(_getReportCallback, _setReportCallback);
    _usb_hid.begin();

    _reenumerate();

    this->_prng.init(0x00);
    this->_crypto.setKey(this->TEA_KEY);
}

void PlayPad::update()
{
    // Update all of our pads every tick as well
    for (int i = 0; i < NUM_PADS; i++)
    {
        this->PADS[i].update();
    }
}

bool PlayPad::_sendReport(uint8_t const *buffer, uint16_t bufsize)
{
    return _usb_hid.sendReport(0, buffer, bufsize);
}

void PlayPad::_configureDevice()
{
    TinyUSBDevice.clearConfiguration();
    TinyUSBDevice.setVersion(0x0100);
    TinyUSBDevice.setID(PLAYPAD_VENDOR_ID, PLAYPAD_PRODUCT_ID);
    TinyUSBDevice.setDeviceVersion(PLAYPAD_VERSION);
    TinyUSBDevice.setManufacturerDescriptor(PLAYPAD_MANUFACTURER);
    TinyUSBDevice.setProductDescriptor(PLAYPAD_PRODUCT);
    TinyUSBDevice.setSerialDescriptor(PLAYPAD_SERIAL);
}

void PlayPad::_reenumerate()
{
    if (TinyUSBDevice.mounted())
    {
        TinyUSBDevice.detach();
        delay(10);
        TinyUSBDevice.attach();
    }
}

uint16_t PlayPad::_getReportCallback(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void PlayPad::_setReportCallback(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void)report_id;
    (void)report_type;

    if (_instance == nullptr)
        return;

    if (bufsize < PLAYPAD_MAX_PACKET_SIZE)
        return;

    switch (static_cast<PacketType>(buffer[0]))
    {
    case PacketType::Command:
    {
        CommandPacket cmd_pkt = CommandPacket();
        memcpy(cmd_pkt.data, buffer, PLAYPAD_MAX_PACKET_SIZE);

        if (cmd_pkt.isValid() != PacketValidationError::OK)
        {
            log_warn("[PlayPad] Received invalid command packet from host (%d): %s", static_cast<uint8_t>(cmd_pkt.isValid()), cmd_pkt.toHexString());
            return;
        }

        _instance->_handleCommandPacket(cmd_pkt);
        break;
    }
    case PacketType::Event:
    {
        log_warn("[PlayPad] Received Event packet from host, which is unexpected. Ignoring.");
        break;
    }
    }
}

void PlayPad::_handleCommandPacket(const CommandPacket &packet)
{
    logger.blinkStatus(2, 100);
    log_dbg("[PlayPad] Valid packet received: Command=0x%02X, PayloadSize=%d, CID=0x%02X",
            packet.command(), packet.payloadSize(), packet.cid());

    auto it = _commandHandlers.find(packet.command());
    if (it != _commandHandlers.end())
    {
        it->second(packet);
    }
    else
    {
        log_warn("[PlayPad] Unhandled command received: 0x%02X", packet.command());
    }
}

void PlayPad::_registerHandlers()
{
    // Add a new line here to register any new command — nothing else needs to change.
    _commandHandlers[GatewayCommand::WAKE] = [this](const CommandPacket &p)
    { _handleWake(p); };
    _commandHandlers[GatewayCommand::SEED] = [this](const CommandPacket &p)
    { _handleSeed(p); };
    _commandHandlers[GatewayCommand::CHALLENGE] = [this](const CommandPacket &p)
    { _handleChallenge(p); };
    _commandHandlers[GatewayCommand::COL] = [this](const CommandPacket &p)
    { _handleCol(p); };
    _commandHandlers[GatewayCommand::COLAL] = [this](const CommandPacket &p)
    { _handleColAll(p); };
    _commandHandlers[GatewayCommand::FLASH] = [this](const CommandPacket &p)
    { _handleFlash(p); };
    _commandHandlers[GatewayCommand::FLSAL] = [this](const CommandPacket &p)
    { _handleFlashAll(p); };
    _commandHandlers[GatewayCommand::FADE] = [this](const CommandPacket &p)
    { _handleFade(p); };
    _commandHandlers[GatewayCommand::FADAL] = [this](const CommandPacket &p)
    { _handleFadeAll(p); };
}

void PlayPad::_handleWake(const CommandPacket &packet)
{
    log_dbg("[PlayPad] WAKE command received. Payload: %s", packet.payloadToHexString());
    ResponsePacket response = ResponsePacket::build(packet.cid(), packet.payload(), packet.payloadSize());
    sendPacket(response);
}

void PlayPad::_handleSeed(const CommandPacket &packet)
{
    log_dbg("[PlayPad] SEED command received. Payload: %s", packet.payloadToHexString());
    SeedPacket::SeedStatus decrypted = SeedPacket::fromCommand(packet, &_crypto);
    log_dbg("[PlayPad] SEED parsed. (Seed: %d, Config: %d)", decrypted.seed, decrypted.conf);

    _prng.init(decrypted.seed);
    ResponsePacket response = SeedPacket::fromStatus(packet.cid(), decrypted, &_crypto);
    sendPacket(response);
}

void PlayPad::_handleChallenge(const CommandPacket &packet)
{
    log_dbg("[PlayPad] CHALLENGE command received. Payload: %s", packet.payloadToHexString());
    ChallengePacket::ChallengeStatus decrypted = ChallengePacket::fromCommand(packet, &_crypto);
    log_dbg("[PlayPad] CHALLENGE parsed. (Config: %d)", decrypted.conf);

    ResponsePacket response = ChallengePacket::fromStatus(packet.cid(), _prng.rand(), decrypted, &_crypto);
    sendPacket(response);
}

void PlayPad::_handleCol(const CommandPacket &packet)
{
    ColorPacket::ColorStatus parsed = ColorPacket::fromCommand(packet);
    log_dbg("[PlayPad] COL parsed; PadIndex: %d (R:%d G:%d B:%d)",
            static_cast<uint8_t>(parsed.padLocation),
            parsed.padColor.r, parsed.padColor.g, parsed.padColor.b);

    if (parsed.padLocation == PadLocation::All)
    {
        for (int i = 0; i < NUM_PADS; i++)
            PADS[i].setColor(parsed.padColor);
    }
    else
    {
        PADS[static_cast<uint8_t>(parsed.padLocation) - 1].setColor(parsed.padColor);
    }

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);
}

void PlayPad::_handleColAll(const CommandPacket &packet)
{
    ColorAllPacket::ColorStatus parsed = ColorAllPacket::fromCommand(packet);
    log_dbg("[PlayPad] COLAL parsed; Left:(R:%d G:%d B:%d) Mid:(R:%d G:%d B:%d) Right:(R:%d G:%d B:%d)",
            parsed.leftColor.r, parsed.leftColor.g, parsed.leftColor.b,
            parsed.centerColor.r, parsed.centerColor.g, parsed.centerColor.b,
            parsed.rightColor.r, parsed.rightColor.g, parsed.rightColor.b);

    PADS[0].setColor(parsed.leftColor);
    PADS[1].setColor(parsed.centerColor);
    PADS[2].setColor(parsed.rightColor);

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);
}

void PlayPad::_handleFlash(const CommandPacket &packet)
{
    FlashPacket::FlashStatus parsed = FlashPacket::fromCommand(packet);
    log_dbg("[PlayPad] FLASH parsed; PadIndex:%d OnTicks:%d OffTicks:%d (R:%d G:%d B:%d)",
            static_cast<uint8_t>(parsed.padLocation),
            parsed.onTicks, parsed.offTicks,
            parsed.offColor.r, parsed.offColor.g, parsed.offColor.b);

    if (parsed.padLocation == PadLocation::All)
    {
        for (int i = 0; i < NUM_PADS; i++)
            PADS[i].setFlash(parsed.offColor, parsed.onTicks, parsed.offTicks, parsed.count);
    }
    else
    {
        PADS[static_cast<uint8_t>(parsed.padLocation) - 1].setFlash(
            parsed.offColor, parsed.onTicks, parsed.offTicks, parsed.count);
    }

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);
}

void PlayPad::_handleFlashAll(const CommandPacket &packet)
{
    log_dbg("[PlayPad] FLSAL command received. Payload: %s", packet.payloadToHexString());
    FlashAllPacket::FlashStatus parsed = FlashAllPacket::fromCommand(packet);

    struct
    {
        int idx;
        FlashAllPacket::FlashStatus::PadFlashStatus &pad;
    } pads[] = {
        {0, parsed.leftPad},
        {1, parsed.centerPad},
        {2, parsed.rightPad},
    };

    for (auto &[idx, pad] : pads)
    {
        if (pad.enable)
            PADS[idx].setFlash(pad.offColor, pad.onTicks, pad.offTicks, pad.count);
    }

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);
}

void PlayPad::_handleFade(const CommandPacket &packet)
{
    log_dbg("[PlayPad] FADE command received. Payload: %s", packet.payloadToHexString());
    FadePacket::FadeStatus parsed = FadePacket::fromCommand(packet);
    log_dbg("[PlayPad] FADE parsed; PadIndex:%d Cycles:%d Speed:%d (R:%d G:%d B:%d)",
            static_cast<uint8_t>(parsed.padLocation),
            parsed.cycles,
            parsed.speed,
            parsed.color.r, parsed.color.g, parsed.color.b);

    if (parsed.padLocation == PadLocation::All)
    {
        for (int i = 0; i < NUM_PADS; i++)
            PADS[i].setFade(parsed.color, parsed.speed, parsed.cycles);
    }
    else
    {
        PADS[static_cast<uint8_t>(parsed.padLocation) - 1].setFade(parsed.color, parsed.speed, parsed.cycles);
    }

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);
}

void PlayPad::_handleFadeAll(const CommandPacket &packet)
{
    log_dbg("[PlayPad] FADEALL command received. Payload: %s", packet.payloadToHexString());
    FadeAllPacket::FadeStatus parsed = FadeAllPacket::fromCommand(packet);

    struct
    {
        int idx;
        FadeAllPacket::FadeStatus::PadFadeStatus &pad;
    } pads[] = {
        {0, parsed.leftPad},
        {1, parsed.centerPad},
        {2, parsed.rightPad},
    };

    for (auto &[idx, pad] : pads)
    {
        if (pad.enable)
            PADS[idx].setFade(pad.color, pad.speed, pad.cycles);
    }

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);
}