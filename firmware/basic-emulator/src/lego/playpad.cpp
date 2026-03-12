#include "lego/playpad.h"
#include "log/logger.h"

PlayPad* PlayPad::_instance = nullptr;

PlayPad::PlayPad() {
    _instance = this;
    this->_registerHandlers();
}

void PlayPad::begin() {
    if (!TinyUSBDevice.isInitialized()) {
        TinyUSBDevice.begin(0);
    }

    _configureDevice();

    _usb_hid.enableOutEndpoint(true);
    _usb_hid.setPollInterval(1);
    _usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
    _usb_hid.setReportCallback(_getReportCallback, _setReportCallback);
    _usb_hid.begin();

    _reenumerate();

    this->_prng.init(0x00);
    this->_crypto.setKey(this->TEA_KEY);
}

void PlayPad::update() {
    // Update all of our pads every tick as well
    for (int i = 0; i < NUM_PADS; i++) {
        this->PADS[i].update();
    }

    // Drain one queued event packet per interval tick
    _eventQueue.update([this](BasePacket& p) { sendPacket(p); });
}

bool PlayPad::_sendReport(uint8_t const* buffer, uint16_t bufsize) {
    return _usb_hid.sendReport(0, buffer, bufsize);
}

void PlayPad::_configureDevice() {
    TinyUSBDevice.clearConfiguration();

    TinyUSBDevice.setVersion(0x0200);

    TinyUSBDevice.setID(PLAYPAD_VENDOR_ID, PLAYPAD_PRODUCT_ID);
    TinyUSBDevice.setDeviceVersion(PLAYPAD_VERSION);
    TinyUSBDevice.setManufacturerDescriptor(PLAYPAD_MANUFACTURER);
    TinyUSBDevice.setProductDescriptor(PLAYPAD_PRODUCT);
    TinyUSBDevice.setSerialDescriptor(PLAYPAD_SERIAL);
}

void PlayPad::_reenumerate() {
    if (TinyUSBDevice.mounted()) {
        TinyUSBDevice.detach();
        delay(10);
        TinyUSBDevice.attach();
    }
}

uint16_t PlayPad::_getReportCallback(uint8_t report_id,
                                     hid_report_type_t report_type,
                                     uint8_t* buffer,
                                     uint16_t reqlen) {
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void PlayPad::_setReportCallback(uint8_t report_id,
                                 hid_report_type_t report_type,
                                 uint8_t const* buffer,
                                 uint16_t bufsize) {
    (void)report_id;
    (void)report_type;

    if (_instance == nullptr) {
        return;
    }

    if (bufsize < PLAYPAD_MAX_PACKET_SIZE) {
        return;
    }

    switch (static_cast<PacketType>(buffer[0])) {
        case PacketType::COMMAND: {
            CommandPacket cmd_pkt = CommandPacket();
            memcpy(cmd_pkt.data, buffer, PLAYPAD_MAX_PACKET_SIZE);

            if (cmd_pkt.isValid() != PacketValidationError::OK) {
                log_warn("[PlayPad] Received invalid command packet from host (%d): %s",
                         static_cast<uint8_t>(cmd_pkt.isValid()), cmd_pkt.toHexString());
                return;
            }

            _instance->_handleCommandPacket(cmd_pkt);
            break;
        }
        case PacketType::EVENT: {
            log_warn("[PlayPad] Received Event packet from host, which is unexpected. Ignoring.");
            break;
        }
    }
}

void PlayPad::_handleCommandPacket(const CommandPacket& packet) {
    log_dbg("[PlayPad] Valid packet received: Command=0x%02X, PayloadSize=%d, CID=0x%02X",
            packet.command(), packet.payloadSize(), packet.cid());

    auto it = _commandHandlers.find(packet.command());
    if (it != _commandHandlers.end()) {
        it->second(packet);
    } else {
        log_warn("[PlayPad] Unhandled command received: 0x%02X", packet.command());
    }
}

void PlayPad::_registerHandlers() {
    // Add a new line here to register any new command - nothing else needs to change.
    _commandHandlers[GatewayCommand::WAKE] = [this](const CommandPacket& p) { _handleWake(p); };
    _commandHandlers[GatewayCommand::SEED] = [this](const CommandPacket& p) { _handleSeed(p); };
    _commandHandlers[GatewayCommand::CHALLENGE] = [this](const CommandPacket& p) {
        _handleChallenge(p);
    };
    _commandHandlers[GatewayCommand::COL] = [this](const CommandPacket& p) { _handleCol(p); };
    _commandHandlers[GatewayCommand::COLAL] = [this](const CommandPacket& p) { _handleColAll(p); };
    _commandHandlers[GatewayCommand::FLASH] = [this](const CommandPacket& p) { _handleFlash(p); };
    _commandHandlers[GatewayCommand::FLSAL] = [this](const CommandPacket& p) {
        _handleFlashAll(p);
    };
    _commandHandlers[GatewayCommand::FADE] = [this](const CommandPacket& p) { _handleFade(p); };
    _commandHandlers[GatewayCommand::FADAL] = [this](const CommandPacket& p) { _handleFadeAll(p); };

    _commandHandlers[GatewayCommand::GETCOL] = [this](const CommandPacket& p) {
        _handleGetColor(p);
    };
    _commandHandlers[GatewayCommand::TGLST] = [this](const CommandPacket& p) { _handleTagList(p); };

    _commandHandlers[GatewayCommand::READ] = [this](const CommandPacket& p) { _handleRead(p); };
    _commandHandlers[GatewayCommand::MODEL] = [this](const CommandPacket& p) { _handleModel(p); };
    _commandHandlers[GatewayCommand::WRITE] = [this](const CommandPacket& p) { _handleWrite(p); };
}

void PlayPad::tagChangeEvent(ToyTag* placedTag, TagIndex lastLocation) {
    uint8_t uid[8];
    placedTag->getUID(uid);

    TagEventPacket::TagMovementDirection direction = TagEventPacket::TagMovementDirection::PLACED;
    TagIndex padIndex = placedTag->padIndex;
    if (placedTag->padIndex == TagIndex::Unplaced && lastLocation != TagIndex::Unplaced) {
        direction = TagEventPacket::TagMovementDirection::REMOVED;
        padIndex = lastLocation;
    }

    const PadLocation currentPadSection = padSectionFromLocation(padIndex);
    if (currentPadSection == PadLocation::ALL) {
        log_err("[PlayPad::tagChangeEvent] No pad section for padIndex=%d",
                static_cast<int>(padIndex));
        return;
    }

    if (direction == TagEventPacket::TagMovementDirection::REMOVED) {
        PlacedToken* pt = _findToken(placedTag);
        if (pt == nullptr) {
            log_err("[PlayPad::tagChangeEvent] Remove: tag id=%d not in tracking table",
                    placedTag->id);
            return;
        }

        log_dbg("[PlayPad::tagChangeEvent] Tag removed - id=%d index=%d pad=%d", placedTag->id,
                pt->index, static_cast<int>(padIndex));

        EventPacket packet = TagEventPacket::build(currentPadSection, pt->index, direction, uid);
        _eventQueue.push(packet);

        _freeIndex(pt->index);
        *pt = _placedTokens[--_placedCount];  // swap-evict
    } else {
        PlacedToken* existing = _findToken(placedTag);

        if (existing != nullptr) {
            const PadLocation oldSection = padSectionFromLocation(existing->padLocation);

            if (oldSection == currentPadSection) {
                // Same section - positional move, keep index
                log_dbg(
                    "[PlayPad::tagChangeEvent] Tag moved within section - id=%d index=%d pad=%d",
                    placedTag->id, existing->index, static_cast<int>(currentPadSection));

                existing->padLocation = padIndex;

                EventPacket packet =
                    TagEventPacket::build(currentPadSection, existing->index, direction, uid);
                _eventQueue.push(packet);
            } else {
                // Cross-section move:
                // Free old index first so the alloc below gets the lowest available,
                // which will be the same index if nothing else is free at a lower slot.
                log_dbg(
                    "[PlayPad::tagChangeEvent] Tag crossed sections - id=%d index=%d pad=%d -> %d",
                    placedTag->id, existing->index, static_cast<int>(oldSection),
                    static_cast<int>(currentPadSection));

                const uint8_t oldIndex = existing->index;

                // 1. REMOVE from old section, free the index
                EventPacket removePacket = TagEventPacket::build(
                    oldSection, oldIndex, TagEventPacket::TagMovementDirection::REMOVED, uid);
                _eventQueue.push(removePacket);
                _freeIndex(oldIndex);

                // 2. Alloc new index (lowest free - hopefully the one *just* freed)
                const uint8_t newIndex = _allocIndex();
                if (newIndex == 0xFF) {
                    log_warn(
                        "[PlayPad::tagChangeEvent] Index table full during cross-section move, "
                        "id=%d",
                        placedTag->id);
                    // Evict the stale slot so the table stays consistent
                    *existing = _placedTokens[--_placedCount];
                    return;
                }

                // 3. Update slot in-place - no evict/reinsert needed
                existing->index = newIndex;
                existing->padLocation = padIndex;

                // 4. PLACE on new section with new index
                EventPacket placePacket = TagEventPacket::build(
                    currentPadSection, newIndex, TagEventPacket::TagMovementDirection::PLACED, uid);
                _eventQueue.push(placePacket);
            }
        } else {
            // Genuinely new placement
            const uint8_t idx = _allocIndex();
            if (idx == 0xFF) {
                log_warn("[PlayPad::tagChangeEvent] Token table full, cannot place id=%d",
                         placedTag->id);
                return;
            }

            _placedTokens[_placedCount++] = {idx, padIndex, placedTag};

            log_dbg("[PlayPad::tagChangeEvent] Tag placed - id=%d index=%d pad=%d", placedTag->id,
                    idx, static_cast<int>(currentPadSection));

            EventPacket packet = TagEventPacket::build(currentPadSection, idx, direction, uid);
            _eventQueue.push(packet);
        }
    }
}

void PlayPad::_notifyPadStateChange() {
    if (!_padStateCallback) {
        return;
    }

    // Re-use the same serialization the GET /playpad endpoint uses
    JsonDocument doc;

    static constexpr struct {
        const char* key;
        PadLocation loc;
    } ENTRIES[] = {
        {"left", PadLocation::LEFT},
        {"center", PadLocation::CENTER},
        {"right", PadLocation::RIGHT},
    };

    for (const auto& e : ENTRIES) {
        JsonObject obj = doc[e.key].to<JsonObject>();
        getPad(e.loc)->toJson(obj);
    }

    String json;
    serializeJson(doc, json);
    _padStateCallback(json);
}

void PlayPad::_handleWake(const CommandPacket& packet) {
    log_dbg("[PlayPad] WAKE command received. Payload: %s", packet.payloadToHexString());

    static const uint8_t WAKE_RESPONSE[] = {0x28, 0x63, 0x29, 0x20, 0x4c, 0x45, 0x47,
                                            0x4f, 0x20, 0x32, 0x30, 0x31, 0x34};

    ResponsePacket response =
        ResponsePacket::build(packet.cid(), WAKE_RESPONSE, sizeof(WAKE_RESPONSE));
    sendPacket(response);
}

void PlayPad::_handleSeed(const CommandPacket& packet) {
    log_dbg("[PlayPad] SEED command received. Payload: %s", packet.payloadToHexString());
    SeedPacket::SeedStatus decrypted = SeedPacket::fromCommand(packet, &_crypto);
    log_dbg("[PlayPad] SEED parsed. (Seed: %d, Config: %d)", decrypted.seed, decrypted.conf);

    _prng.init(decrypted.seed);
    ResponsePacket response = SeedPacket::fromStatus(packet.cid(), decrypted, &_crypto);
    sendPacket(response);
}

void PlayPad::_handleChallenge(const CommandPacket& packet) {
    log_dbg("[PlayPad] CHALLENGE command received. Payload: %s", packet.payloadToHexString());
    ChallengePacket::ChallengeStatus decrypted = ChallengePacket::fromCommand(packet, &_crypto);
    log_dbg("[PlayPad] CHALLENGE parsed. (Config: %d)", decrypted.conf);

    ResponsePacket response =
        ChallengePacket::fromStatus(packet.cid(), _prng.rand(), decrypted, &_crypto);
    sendPacket(response);
}

void PlayPad::_handleCol(const CommandPacket& packet) {
    ColorPacket::ColorStatus parsed = ColorPacket::fromCommand(packet);
    log_dbg("[PlayPad] COL parsed; PadIndex: %d (R:%d G:%d B:%d)",
            static_cast<uint8_t>(parsed.padLocation), parsed.padColor.r, parsed.padColor.g,
            parsed.padColor.b);

    if (parsed.padLocation == PadLocation::ALL) {
        for (int i = 0; i < NUM_PADS; i++) {
            PADS[i].setColor(parsed.padColor);
        }
    } else {
        PADS[static_cast<uint8_t>(parsed.padLocation) - 1].setColor(parsed.padColor);
    }

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);

    _notifyPadStateChange();
}

void PlayPad::_handleColAll(const CommandPacket& packet) {
    ColorAllPacket::ColorStatus parsed = ColorAllPacket::fromCommand(packet);
    log_dbg(
        "[PlayPad] COLAL parsed; Left:(R:%d G:%d B:%d) Mid:(R:%d G:%d B:%d) Right:(R:%d G:%d B:%d)",
        parsed.leftColor.r, parsed.leftColor.g, parsed.leftColor.b, parsed.centerColor.r,
        parsed.centerColor.g, parsed.centerColor.b, parsed.rightColor.r, parsed.rightColor.g,
        parsed.rightColor.b);

    PADS[0].setColor(parsed.leftColor);
    PADS[1].setColor(parsed.centerColor);
    PADS[2].setColor(parsed.rightColor);

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);

    _notifyPadStateChange();
}

void PlayPad::_handleFlash(const CommandPacket& packet) {
    FlashPacket::FlashStatus parsed = FlashPacket::fromCommand(packet);
    log_dbg("[PlayPad] FLASH parsed; PadIndex:%d OnTicks:%d OffTicks:%d (R:%d G:%d B:%d)",
            static_cast<uint8_t>(parsed.padLocation), parsed.onTicks, parsed.offTicks,
            parsed.offColor.r, parsed.offColor.g, parsed.offColor.b);

    if (parsed.padLocation == PadLocation::ALL) {
        for (int i = 0; i < NUM_PADS; i++) {
            PADS[i].setFlash(parsed.offColor, parsed.onTicks, parsed.offTicks, parsed.count);
        }
    } else {
        PADS[static_cast<uint8_t>(parsed.padLocation) - 1].setFlash(parsed.offColor, parsed.onTicks,
                                                                    parsed.offTicks, parsed.count);
    }

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);

    _notifyPadStateChange();
}

void PlayPad::_handleFlashAll(const CommandPacket& packet) {
    log_dbg("[PlayPad] FLSAL command received. Payload: %s", packet.payloadToHexString());
    FlashAllPacket::FlashStatus parsed = FlashAllPacket::fromCommand(packet);

    struct {
        int idx;
        FlashAllPacket::FlashStatus::PadFlashStatus& pad;
    } pads[] = {
        {0, parsed.leftPad},
        {1, parsed.centerPad},
        {2, parsed.rightPad},
    };

    for (auto& [idx, pad] : pads) {
        if (pad.enable) {
            PADS[idx].setFlash(pad.offColor, pad.onTicks, pad.offTicks, pad.count);
        }
    }

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);

    _notifyPadStateChange();
}

void PlayPad::_handleFade(const CommandPacket& packet) {
    log_dbg("[PlayPad] FADE command received. Payload: %s", packet.payloadToHexString());
    FadePacket::FadeStatus parsed = FadePacket::fromCommand(packet);
    log_dbg("[PlayPad] FADE parsed; PadIndex:%d Cycles:%d Speed:%d (R:%d G:%d B:%d)",
            static_cast<uint8_t>(parsed.padLocation), parsed.cycles, parsed.speed, parsed.color.r,
            parsed.color.g, parsed.color.b);

    if (parsed.padLocation == PadLocation::ALL) {
        for (int i = 0; i < NUM_PADS; i++) {
            PADS[i].setFade(parsed.color, parsed.speed, parsed.cycles);
        }
    } else {
        PADS[static_cast<uint8_t>(parsed.padLocation) - 1].setFade(parsed.color, parsed.speed,
                                                                   parsed.cycles);
    }

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);

    _notifyPadStateChange();
}

void PlayPad::_handleFadeAll(const CommandPacket& packet) {
    log_dbg("[PlayPad] FADEALL command received. Payload: %s", packet.payloadToHexString());
    FadeAllPacket::FadeStatus parsed = FadeAllPacket::fromCommand(packet);

    struct {
        int idx;
        FadeAllPacket::FadeStatus::PadFadeStatus& pad;
    } pads[] = {
        {0, parsed.leftPad},
        {1, parsed.centerPad},
        {2, parsed.rightPad},
    };

    for (auto& [idx, pad] : pads) {
        if (pad.enable) {
            PADS[idx].setFade(pad.color, pad.speed, pad.cycles);
        }
    }

    ResponsePacket blank = ResponsePacket::blank(packet.cid());
    sendPacket(blank);

    _notifyPadStateChange();
}

void PlayPad::_handleGetColor(const CommandPacket& packet) {
    const PadLocation colorToGet = GetColorPacket::fromCommand(packet);

    log_dbg("[PlayPad] Getting color for pad %d", colorToGet);
    ResponsePacket response = GetColorPacket::buildResponse(
        packet.cid(), PADS[static_cast<uint8_t>(colorToGet) - 1].color());

    sendPacket(response);
}

void PlayPad::_handleTagList(const CommandPacket& packet) {
    TagListPacket::TagEntry tagEntries[MAX_TOKENS]{};

    for (uint8_t i = 0; i < _placedCount; i++) {
        PlacedToken* tag = &_placedTokens[i];
        tagEntries[i] = {tag->index, padSectionFromLocation(tag->padLocation)};
    }

    ResponsePacket response = TagListPacket::buildResponse(packet.cid(), tagEntries, _placedCount);
    log_dbg("[PlayPad] TGLST Response");
    sendPacket(response);
}

void PlayPad::_handleRead(const CommandPacket& packet) {
    const ReadPacket::ReadRequest req = ReadPacket::fromCommand(packet);

    log_dbg("[PlayPad] READ command // index=%d page=%d", req.index, req.page);

    // Find the placed token matching this index
    const PlacedToken* pt = nullptr;
    for (uint8_t i = 0; i < _placedCount; i++) {
        if (_placedTokens[i].index == req.index) {
            pt = &_placedTokens[i];
            break;
        }
    }

    if (pt == nullptr) {
        log_warn("[PlayPad] READ - no token available at index=%d", req.index);
        return;
    }

    ResponsePacket response = ReadPacket::buildResponse(packet.cid(), pt->tag->_buf, req.page);
    log_dbg("[PlayPad] READ - // Responding with tag id=%d page=%d", pt->tag->id, req.page);
    sendPacket(response);
}

void PlayPad::_handleModel(const CommandPacket& packet) {
    const ModelPacket::ModelRequest req = ModelPacket::fromCommand(packet, &_crypto);

    log_dbg("[PlayPad] MODEL command - index=%d conf=0x%08X", req.index, req.conf);

    // Find the placed token matching this index
    const PlacedToken* pt = nullptr;
    for (uint8_t i = 0; i < _placedCount; i++) {
        if (_placedTokens[i].index == req.index) {
            pt = &_placedTokens[i];
            break;
        }
    }

    if (pt == nullptr) {
        log_warn("[PlayPad] MODEL - no token at index=%d", req.index);
        ResponsePacket err = ModelPacket::buildResponse(packet.cid(), 0, req.conf,
                                                        ModelPacket::STATUS_NO_TOKEN, &_crypto);
        sendPacket(err);
        return;
    }

    const uint16_t tagId = pt->tag->id;

    if (tagId == 0) {
        log_warn("[PlayPad] MODEL - token at index=%d has no id", req.index);
        ResponsePacket noId = ModelPacket::buildResponse(packet.cid(), 0, req.conf,
                                                         ModelPacket::STATUS_NO_ID, &_crypto);
        sendPacket(noId);
        return;
    }

    log_dbg("[PlayPad] MODEL - responding with id=%d for index=%d", tagId, req.index);
    ResponsePacket response =
        ModelPacket::buildResponse(packet.cid(), tagId, req.conf, ModelPacket::STATUS_OK, &_crypto);
    sendPacket(response);
}

void PlayPad::_handleWrite(const CommandPacket& packet) {
    const WritePacket::WriteRequest req = WritePacket::fromCommand(packet);

    log_dbg("[PlayPad] WRITE command - index=%d page=%d data=%02X%02X%02X%02X", req.index, req.page,
            req.data[0], req.data[1], req.data[2], req.data[3]);

    ResponsePacket response = WritePacket::buildResponse(packet.cid());
    sendPacket(response);

    PlacedToken* pt = nullptr;
    for (uint8_t i = 0; i < _placedCount; i++) {
        if (_placedTokens[i].index == req.index) {
            pt = &_placedTokens[i];
            break;
        }
    }

    if (pt == nullptr) {
        log_warn("[PlayPad] WRITE - no token at index=%d", req.index);
        return;
    }

    // Write the primary page
    const uint8_t byteOffset = req.page * NFCTag::PAGE_SIZE;
    if (byteOffset + NFCTag::PAGE_SIZE > NFCTag::TAG_SIZE) {
        log_warn("[PlayPad] WRITE - page=%d out of range for index=%d", req.page, req.index);
        return;
    }

    memcpy(pt->tag->_buf + byteOffset, req.data, NFCTag::PAGE_SIZE);

    // Mirror to the paired page so reads/writes on either offset stay consistent
    // 23 <-> 35  |  24 <-> 36  |  25 <-> 37
    uint8_t mirrorPage = 0xFF;
    switch (req.page) {
        case 23:
            mirrorPage = 35;
            break;
        case 35:
            mirrorPage = 23;
            break;
        case 24:
            mirrorPage = 36;
            break;
        case 36:
            mirrorPage = 24;
            break;
        case 25:
            mirrorPage = 37;
            break;
        case 37:
            mirrorPage = 25;
            break;
        default:
            break;
    }

    if (mirrorPage != 0xFF) {
        const uint8_t mirrorOffset = mirrorPage * NFCTag::PAGE_SIZE;
        memcpy(pt->tag->_buf + mirrorOffset, req.data, NFCTag::PAGE_SIZE);
        log_dbg("[PlayPad] WRITE - mirrored page=%d -> page=%d", req.page, mirrorPage);
    }

    log_dbg("[PlayPad] WRITE - wrote page=%d to tag id=%d", req.page, pt->tag->id);

    if (pt->tag->type == ToyTagType::Vehicle) {
        VehicleTag* vehicle = static_cast<VehicleTag*>(pt->tag);

        switch (req.page) {
            case 23:
            case 35:
            case 24:
            case 36:
            case 25:
            case 37:
                vehicle->readFromBuffer();
                log_dbg(
                    "[PlayPad] WRITE - vehicle fields resynced: id=%d upg23=0x%08X upg25=0x%08X",
                    vehicle->id, vehicle->upgradesP23, vehicle->upgradesP25);
                break;
            default:
                break;
        }
    }
}