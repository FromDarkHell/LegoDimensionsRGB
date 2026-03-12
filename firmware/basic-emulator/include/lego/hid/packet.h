#pragma once

#include "lego/crypto/tea.h"
#include "lego/hid/constants.h"
#include "util/reader.h"

enum class PacketValidationError : uint8_t {
    OK = 0x00,
    BAD_TYPE = 0x01,
    MAGIC = 0x02,
    LENGTH = 0x03,
    CHECKSUM = 0x04
};

/**
 * @brief BasePacket is for all other packet types to implement, it holds an instance for both the
 * command type, as well as payload info, encryption, and checksumming.
 *
 */
struct BasePacket {
    uint8_t data[PLAYPAD_MAX_PACKET_SIZE];

    BasePacket() { memset(data, 0x00, PLAYPAD_MAX_PACKET_SIZE); }

    PacketType type() const { return static_cast<PacketType>(data[0]); }
    uint8_t length() const { return data[1]; }
    GatewayCommand command() const { return static_cast<GatewayCommand>(data[2]); }
    uint8_t checksum() const { return data[length() + 2]; }
    virtual const uint8_t* payload() const = 0;
    virtual uint8_t payloadSize() const = 0;

    uint8_t computeChecksum() const {
        uint16_t sum = 0;
        for (uint8_t i = 0; i < length() + 2; ++i) {
            sum += data[i];
        }
        return static_cast<uint8_t>(sum & 0xFF);
    }

    PacketValidationError isValid(PacketType expectedType) const {
        if (type() != expectedType) {
            return PacketValidationError::BAD_TYPE;
        }

        if (length() + 2u >= PLAYPAD_MAX_PACKET_SIZE) {
            return PacketValidationError::LENGTH;
        }

        if (computeChecksum() != checksum()) {
            return PacketValidationError::CHECKSUM;
        }

        return PacketValidationError::OK;
    }

    const char* toHexString() const {
        static char hexString[PLAYPAD_MAX_PACKET_SIZE * 2 + 1];
        for (size_t i = 0; i < PLAYPAD_MAX_PACKET_SIZE; ++i) {
            sprintf(&hexString[i * 2], "%02X", data[i]);
        }
        return hexString;
    }

    const char* payloadToHexString() const {
        static char hexString[PLAYPAD_MAX_PACKET_SIZE * 2 + 1];
        for (size_t i = 0; i < payloadSize(); ++i) {
            sprintf(&hexString[i * 2], "%02X", payload()[i]);
        }
        return hexString;
    }

    // Encrypts the payload in-place and recomputes the checksum.
    // Returns false if TEA encryption fails or there is no payload.
    bool encryptPayload(TEA* tea) {
        if (payloadSize() == 0) {
            return false;
        }

        uint8_t encrypted[PLAYPAD_MAX_PACKET_SIZE]{};
        if (!tea->encrypt(payload(), encrypted)) {
            return false;
        }

        memcpy(&data[4], encrypted, payloadSize());
        data[payloadSize() + 4] = computeChecksum();
        return true;
    }

    // Decrypts the payload and returns the decrypted array
    // Returns nullptr if TEA decryption fails or there is no payload.
    static const uint8_t* decryptPayload(const BasePacket* packet, TEA* tea) {
        if (packet->payloadSize() == 0) {
            return nullptr;
        }

        uint8_t* decrypted = (uint8_t*)malloc(PLAYPAD_MAX_PACKET_SIZE);
        if (!tea->decrypt(packet->payload(), decrypted)) {
            return nullptr;
        }

        return decrypted;
    }

   protected:
    ~BasePacket() = default;
};

/**
 * @brief A command packet comes from the game to tell the playpad to do something specific. A
 * response packet will (usually) be sent in return. Each command comes with a Command ID (`cid`),
 * which is used to tell the game that the playpad is responding to a specific command.
 *
 * The data is laid out like so: `[type][length][cmd][cid][...payload][checksum]`
 */
struct CommandPacket : BasePacket {
    uint8_t cid() const { return data[3]; }
    const uint8_t* payload() const { return &data[4]; }
    uint8_t payloadSize() const { return length() > 2 ? length() - 2 : 0; }

    PacketValidationError isValid() const { return BasePacket::isValid(PacketType::COMMAND); }

    static CommandPacket build(GatewayCommand cmd,
                               uint8_t cid,
                               const uint8_t* payload = nullptr,
                               uint8_t payloadLen = 0) {
        assert(payloadLen <= PLAYPAD_MAX_PACKET_SIZE - 5);

        CommandPacket pkt;
        pkt.data[0] = static_cast<uint8_t>(PacketType::COMMAND);
        pkt.data[1] = payloadLen + 2;
        pkt.data[2] = static_cast<uint8_t>(cmd);
        pkt.data[3] = cid;

        if (payload && payloadLen > 0) {
            memcpy(&pkt.data[4], payload, payloadLen);
        }

        pkt.data[payloadLen + 4] = pkt.computeChecksum();
        return pkt;
    }
};

/**
 * @brief An event packet is a simple wrapper around the BasePacket, used for when the playpad has
 * an update (i.e. an NFC tag has been placed). Unlike `CommandPacket`, an EventPacket doesn't have
 * a `cid`.
 *
 * The packet is laid out like so: `[type][length][cmd][...payload][checksum]`
 */
struct EventPacket : BasePacket {
    const uint8_t* payload() const { return &data[3]; }
    uint8_t payloadSize() const { return length() > 1 ? length() - 1 : 0; }

    PacketValidationError isValid() const { return BasePacket::isValid(PacketType::EVENT); }
};

/**
 * @brief A response packet is a response send from the playpad back to the console, in order to
 * tell the game specific info about a given command
 *
 * This is laid out similarly to the CommandPacket, but it doesn't need a command type since it is a
 * response to a given packet that was sent.
 *
 * For example: `[type][length][cid][...payload][checksum]`
 */
struct ResponsePacket : BasePacket {
    uint8_t cid() const { return data[2]; }
    const uint8_t* payload() const override { return &data[3]; }
    uint8_t payloadSize() const override { return length() > 1 ? length() - 1 : 0; }

    PacketValidationError isValid() const {
        return BasePacket::isValid(PacketType::COMMAND);  // 0x55 for both cmd and response
    }

    /**
     * @brief Crafts a base ResponsePacket with a given CID and payload
     *
     * @param cid The originating command's `cid` value to use as an ID for this response.
     * @param payload The payload data to return, defaults to nullptr/blank.
     * @param payloadLen The length of the payload data to return
     * @return ResponsePacket A fully serialized, checksummed, ResponsePacket.
     */
    static ResponsePacket build(uint8_t cid,
                                const uint8_t* payload = nullptr,
                                uint8_t payloadLen = 0) {
        assert(payloadLen <= PLAYPAD_MAX_PACKET_SIZE - 4);

        ResponsePacket pkt;
        pkt.data[0] = static_cast<uint8_t>(PacketType::COMMAND);
        pkt.data[1] = payloadLen + 1;  // length = cid + payload
        pkt.data[2] = cid;

        if (payload && payloadLen > 0) {
            memcpy(&pkt.data[3], payload, payloadLen);
        }

        pkt.data[payloadLen + 3] = pkt.computeChecksum();
        return pkt;
    }

    /**
     * @brief Crafts a blank (payload-less) ResponsePacket using a given `cid`.
     *
     * @param cid The originating command's `cid` value to use as an ID for this response.
     * @return ResponsePacket A fully serialized, checksummed, ResponsePacket.
     */
    static ResponsePacket blank(uint8_t cid) {
        ResponsePacket pkt;
        pkt.data[0] = static_cast<uint8_t>(PacketType::COMMAND);
        pkt.data[1] = 1;  // length = cid
        pkt.data[2] = cid;

        pkt.data[3] = pkt.computeChecksum();
        return pkt;
    }
};
