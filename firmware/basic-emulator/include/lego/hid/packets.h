#include "lego/hid/packet.h"
#include "lego/hid/pad.h"

/**
 * @brief A SEED packet is used to seed the PRNG, as well as a part of the authentication process.
 * The main payload (first 8 bytes) of the packet are encrypted via TEA.
 *
 */
struct SeedPacket : CommandPacket
{
    static constexpr uint8_t PAYLOAD_SIZE = 8;

    // Parsed/decrypted contents of a SEED command payload
    struct SeedStatus
    {
        uint32_t seed; // read as LE - burtle.init(seed)
        uint32_t conf; // read as BE - echoed back in response
    };

    /// @brief Parses and decrypts an incoming SEED command from the device.
    /// @param packet Raw CommandPacket received from the device
    /// @param crypto TEA instance with the shared key already set
    /// @return Decrypted SeedStatus, or {0,0} on failure
    static SeedStatus fromCommand(const CommandPacket &packet, TEA *crypto)
    {
        const uint8_t *decrypted = BasePacket::decryptPayload(&packet, crypto);
        if (!decrypted)
            return {0, 0};

        const uint32_t seed = Reader::readUInt32LE(decrypted, 0x00);
        const uint32_t conf = Reader::readUInt32BE(decrypted, 0x04);

        free((void *)decrypted);
        return {seed, conf};
    }

    /// @brief Builds an encrypted SEED response to send back to the device.
    ///        Response payload: [conf_u32be][0x00 x4] encrypted
    /// @param cid    CID mirrored from the incoming command
    /// @param status SeedStatus from fromCommand()
    /// @param crypto TEA instance with the shared key already set
    /// @return ResponsePacket
    static ResponsePacket fromStatus(uint8_t cid, const SeedStatus &status, TEA *crypto)
    {
        uint8_t payload[PAYLOAD_SIZE]{};
        Reader::writeUInt32BE(payload, 0x00, status.conf); // conf as BE at offset 0

        // Encrypt the full 8-byte block before building the packet
        uint8_t encrypted[PAYLOAD_SIZE]{};
        if (!crypto->encrypt(payload, encrypted))
            return {};

        ResponsePacket pkt = ResponsePacket::build(cid, encrypted, PAYLOAD_SIZE);
        return pkt;
    }
};

/**
 * @brief A CHALLENGE packet is used to help authenticate that this pad is a "legal" playpad.
 * The packet payload comes encrypted through TEA, & needs to be decrypted. Afterwards, it includes a uint32 `conf` key.
 * The seeded RNG will generate a random number, and send it back as a response. This proves both the PRNG is implemented correctly, as well as TEA
 * So it *obviously* has to be an official playpad...
 *
 */
struct ChallengePacket : CommandPacket
{
    static constexpr uint8_t PAYLOAD_SIZE = 8;

    // Parsed/decrypted contents of a CHALLENGE command payload
    struct ChallengeStatus
    {
        uint32_t conf; // read as BE - echoed back in response
    };

    /// @brief Parses and decrypts an incoming SEED command from the device.
    /// @param packet Raw CommandPacket received from the device
    /// @param crypto TEA instance with the shared key already set
    /// @return Decrypted ChallengeStatus, or {0xFFFFFFFF} on failure
    static ChallengeStatus fromCommand(const CommandPacket &packet, TEA *crypto)
    {
        const uint8_t *decrypted = BasePacket::decryptPayload(&packet, crypto);
        if (!decrypted)
            return {0xFFFFFFFF};

        const uint32_t conf = Reader::readUInt32BE(decrypted, 0x00);

        free((void *)decrypted);
        return {conf};
    }

    /// @brief Builds an encrypted SEED response to send back to the device.
    ///        Response payload: [conf_u32be][0x00 x4] encrypted
    /// @param cid    CID mirrored from the incoming command
    /// @param rand   A random PRNG number generated from Burtle
    /// @param status ChallengeStatus from fromCommand()
    /// @param crypto TEA instance with the shared key already set
    /// @return ResponsePacket
    static ResponsePacket fromStatus(uint8_t cid, uint32_t rand, const ChallengeStatus &status, TEA *crypto)
    {
        uint8_t payload[PAYLOAD_SIZE]{};
        Reader::writeUInt32LE(payload, 0x00, rand);
        Reader::writeUInt32BE(payload, 0x04, status.conf);

        // Encrypt the full 8-byte block before building the packet
        uint8_t encrypted[PAYLOAD_SIZE]{};
        if (!crypto->encrypt(payload, encrypted))
            return {};

        ResponsePacket pkt = ResponsePacket::build(cid, encrypted, PAYLOAD_SIZE);
        return pkt;
    }
};

/**
 * @brief A COLOR packet changes the color of a specific playpad
 *
 */
struct ColorPacket : CommandPacket
{
    // Parsed/decrypted contents of a COLOR command payload
    struct ColorStatus
    {
        PadLocation padLocation;
        PadColor padColor;
    };

    /// @brief Parses an incoming COLOR command from the device.
    /// @param packet Raw CommandPacket received from the device
    /// @return A parsed COLOR command
    static ColorStatus fromCommand(const CommandPacket &packet)
    {
        const uint8_t *decrypted = packet.payload();

        const PadLocation padIndex = static_cast<PadLocation>(decrypted[0]);
        const PadColor padColor = PadColor::fromBuffer(&decrypted[1]);

        return {padIndex, padColor};
    }
};

/**
 * @brief A COLAL packet lets you change the colors of all of the specific playpads in a single packet.
 *
 */
struct ColorAllPacket : CommandPacket
{
    // Parsed/decrypted contents of a COLORALL command payload
    struct ColorStatus
    {
        PadColor leftColor;
        PadColor centerColor;
        PadColor rightColor;
    };

    /// @brief Parses an incoming COLORALL command from the device.
    /// @param packet Raw CommandPacket received from the device
    /// @return A parsed COLOR command
    static ColorStatus fromCommand(const CommandPacket &packet)
    {
        const uint8_t *decrypted = packet.payload();

        return {
            PadColor::fromBuffer(&decrypted[1]),
            PadColor::fromBuffer(&decrypted[2]),
            PadColor::fromBuffer(&decrypted[3])};
    }
};

/**
 * @brief A FLSH packet lets you flash a specific playpad to the current color for `onTicks` ticks, and then flash to `offColor` for `offTicks` ticks. It does this cycle `count` number of times.
 *
 * A tick is defined as ~100ms.
 *
 */
struct FlashPacket : CommandPacket
{
    // Parsed/decrypted contents of a CHALLENGE command payload
    struct FlashStatus
    {
        PadLocation padLocation;
        uint8_t onTicks;
        uint8_t offTicks;
        uint8_t count;

        PadColor offColor;
    };

    /// @brief Parses an incoming FLSH command from the device.
    /// @param packet Raw CommandPacket received from the device
    /// @return A parsed FLSH command
    static FlashStatus fromCommand(const CommandPacket &packet)
    {
        const uint8_t *decrypted = packet.payload();

        const PadLocation padIndex = static_cast<PadLocation>(decrypted[0]);
        const PadColor padColor = PadColor::fromBuffer(&decrypted[4]);

        return {
            padIndex,
            decrypted[1],
            decrypted[2],
            decrypted[3],

            padColor};
    }
};

/**
 * @brief A FLSHALL packet lets you flash all of the playpads in a single packet. This functions equivalently to the FlashPacket, but with all of their data combined.
 *
 */
struct FlashAllPacket : CommandPacket
{
    struct FlashStatus
    {
        struct PadFlashStatus
        {
            uint8_t enable;
            uint8_t onTicks;
            uint8_t offTicks;
            uint8_t count;

            PadColor offColor;
        };

        PadFlashStatus centerPad;
        PadFlashStatus leftPad;
        PadFlashStatus rightPad;
    };

    /// @brief Parses an incoming FLSHALL command from the device.
    /// @param packet Raw CommandPacket received from the device
    /// @return A parsed FLSHALL command
    static FlashStatus fromCommand(const CommandPacket &packet)
    {
        const uint8_t *decrypted = packet.payload();
        FlashStatus result;

        for (int i = 0; i < 3; i++)
        {
            int offset = (i * 7);
            const PadColor padColor = PadColor::fromBuffer(&decrypted[offset + 4]);

            // (EN: 01, ON_TICK: 0A, OFF_TICK: 14, COUNT: 0A, COLOR: FF0000)
            // (EN: 01, ON_TICK: 0A, OFF_TICK: 14, COUNT: 0F, COLOR: 00FF00)
            // (EN: 01, ON_TICK: 0A, OFF_TICK: 14, COUNT: 14, COLOR: 0000FF)
            FlashStatus::PadFlashStatus flashStatus = {
                decrypted[offset + 0],
                decrypted[offset + 1],
                decrypted[offset + 2],
                decrypted[offset + 3],
                padColor};

            if (i == 0)
                result.centerPad = flashStatus;
            else if (i == 1)
                result.leftPad = flashStatus;
            else if (i == 2)
                result.rightPad = flashStatus;
        }

        return result;
    }
};

/**
 * @brief A FADE packet lets you fade from the current color to the next color x amount of times.
 *
 */
struct FadePacket : CommandPacket
{
    struct FadeStatus
    {
        PadLocation padLocation;
        uint8_t speed;
        uint8_t cycles;

        PadColor color;
    };

    /// @brief Parses an incoming FADE command from the device.
    /// @param packet Raw CommandPacket received from the device
    /// @return A parsed FADE command
    static FadeStatus fromCommand(const CommandPacket &packet)
    {
        const uint8_t *decrypted = packet.payload();

        const PadLocation padIndex = static_cast<PadLocation>(decrypted[0]);
        const PadColor padColor = PadColor::fromBuffer(&decrypted[3]);

        return {
            padIndex,
            decrypted[1],
            decrypted[2],
            padColor};
    }
};

/**
 * @brief A FADEALL packet lets you fade all of the playpads in a single packet. This functions equivalently to the FadePacket, but with all of their data combined.
 *
 */
struct FadeAllPacket : CommandPacket
{
    struct FadeStatus
    {
        struct PadFadeStatus
        {
            uint8_t enable;
            uint8_t speed;
            uint8_t cycles;

            PadColor color;
        };

        PadFadeStatus centerPad;
        PadFadeStatus leftPad;
        PadFadeStatus rightPad;
    };

    /// @brief Parses an incoming FADEALL command from the device.
    /// @param packet Raw CommandPacket received from the device
    /// @return A parsed FADEALL command
    static FadeStatus fromCommand(const CommandPacket &packet)
    {
        const uint8_t *decrypted = packet.payload();
        FadeStatus result;

        for (int i = 0; i < 3; i++)
        {
            int offset = (i * 6);
            const PadColor padColor = PadColor::fromBuffer(&decrypted[offset + 3]);

            FadeStatus::PadFadeStatus fadeStatus = {
                decrypted[offset + 0],
                decrypted[offset + 1],
                decrypted[offset + 2],
                padColor};

            if (i == 0)
                result.centerPad = fadeStatus;
            else if (i == 1)
                result.leftPad = fadeStatus;
            else if (i == 2)
                result.rightPad = fadeStatus;
        }

        return result;
    }
};
