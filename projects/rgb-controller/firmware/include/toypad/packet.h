#pragma once

#include "constants.h"

/**
 * @brief A command packet the host (game) sends to the playpad.
 *
 * Layout: [type=0x55][length][cmd][cid][...payload][checksum][padding]
 *
 * We only gotta to *build* packets for this project so like, this is all we need.
 */
struct CommandPacket {
    uint8_t data[PLAYPAD_MAX_PACKET];

    CommandPacket() { memset(data, 0x00, PLAYPAD_MAX_PACKET); }

    /**
     * @brief Compute the checksum over bytes [0 .. length+1] inclusive.
     */
    uint8_t computeChecksum() const {
        uint16_t sum = 0;
        // data[1] == length; the checksum covers [type, length, cmd, cid, payload]
        for (uint8_t i = 0; i < (uint8_t)(data[1] + 2); i++) {
            sum += data[i];
        }
        return static_cast<uint8_t>(sum & 0xFF);
    }

    /**
     * @brief Build a fully-checksummed, zero-padded command packet.
     *
     * @param cmd        ToypadCommand opcode
     * @param cid        Command ID (echoed in the pad's response)
     * @param platform   Specific platform to generate this packet for
     * @param payload    Optional payload bytes
     * @param payloadLen Number of payload bytes (max PLAYPAD_MAX_PACKET - 5)
     */
    static CommandPacket build(ToypadCommand cmd,
                               uint8_t cid,
                               ToypadPlatform platform,
                               const uint8_t* payload = nullptr,
                               uint8_t payloadLen = 0) {
        CommandPacket pkt;
        pkt.data[0] = static_cast<uint8_t>(PacketType::COMMAND);  // 0x55
        pkt.data[1] = payloadLen + 2;                             // length = cmd + cid + payload
        pkt.data[2] = static_cast<uint8_t>(cmd);
        pkt.data[3] = cid;

        if (payload && payloadLen > 0) {
            memcpy(&pkt.data[4], payload, payloadLen);
        }

        pkt.data[payloadLen + 4] = pkt.computeChecksum();

        // All X360 packets just slap an extra few bytes at the start regardless of the
        // checksum/etc.
        if (platform == ToypadPlatform::X360) {
            memmove(&pkt.data[2], &pkt.data[0], payloadLen + 5);
            pkt.data[0] = 0x0B;
            pkt.data[1] = 0x16;
        }

        // Remaining bytes stay 0x00 (padding already zeroed by constructor)
        return pkt;
    }
};