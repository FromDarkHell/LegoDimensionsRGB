#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "lego/hid/nfctag.h"
#include "util/reader.h"

/**
 * @brief An enum detailing the specific types that a toytag can be
 *
 */
enum class ToyTagType : uint8_t { Unknown = 0, Character = 1, Vehicle = 2 };

/**
 * @brief An enum detailing all of the locations/values that a tag can be on the playpads
 *
 */
enum class TagIndex : int8_t {
    INVALID = 0x00,

    TopLeft = 0x01,
    Middle = 0x02,
    TopRight = 0x03,

    BottomLeft = 0x04,
    CenterLeft = 0x05,
    CenterRight = 0x06,
    BottomRight = 0x07,

    Unplaced = -1
};

/**
 * @brief Base class for all Lego Dimensions toy tags.
 *        Extends NFCTag with game metadata (id, name, padIndex, type)
 *        as well as JSON serialization / deserialization.
 */
class ToyTag : public NFCTag {
   public:
    /**
     * @brief An ID describing the tag in the JSON database of characters/vehicles
     *
     */
    uint16_t id;

    /**
     * @brief A string describing the tags name, used for display/serialization purposes
     *
     */
    char name[48];

    /**
     * @brief The current location of the tag on the playpad
     *
     */
    TagIndex padIndex;

    /**
     * @brief The type of this toytag (Vehicle or Character)
     *
     */
    ToyTagType type;

    ToyTag() : NFCTag(), id(0), padIndex(TagIndex::Unplaced), type(ToyTagType::Unknown) {
        name[0] = '\0';
        setUID(randomUID());  // Generated once, lives in _buf permanently
    }

    virtual JsonDocument toJSON() const {
        JsonDocument doc;
        fillJSON(doc);
        return doc;
    }

    /**
     * @brief Restores all of the game-specific metadata from a JSONVariant
     *
     */
    virtual bool fromJSON(const JsonVariant input) {
        parseJSON(input);
        return true;
    }

   protected:
    /**
     * @brief Used to fill JSON content for serialization, should be overridden in subclasses to add
     * in extra fields, etc
     *
     * @param doc
     */
    virtual void fillJSON(JsonDocument& doc) const {
        doc["name"] = name;
        doc["id"] = id;

        char uidBuf[15];
        this->getUIDStr(uidBuf);
        doc["uid"] = uidBuf;

        doc["index"] = static_cast<int16_t>(padIndex);
        doc["type"] = typeString();
    }

    /**
     * @brief Does basic parsing of a JSON document to a ToyTag instance
     *
     * @param doc The JSON serialized data to load
     */
    virtual void parseJSON(const JsonVariant doc) {
        id = doc["id"] | 0;
        padIndex = static_cast<TagIndex>(doc["index"] | 0x00);
        strlcpy(name, doc["name"] | "", sizeof(name));

        const char* typeStr = doc["type"] | "";
        if (strcmp(typeStr, "character") == 0) {
            type = ToyTagType::Character;
        } else if (strcmp(typeStr, "vehicle") == 0) {
            type = ToyTagType::Vehicle;
        } else {
            type = ToyTagType::Unknown;
        }

        this->setUID(doc["uid"] | "");
    }

    /**
     * @brief Gets the current type of the toytag as a string, used for JSON serialization
     *
     * @return const char*
     */
    const char* typeString() const {
        switch (type) {
            case ToyTagType::Character:
                return "character";
            case ToyTagType::Vehicle:
                return "vehicle";
            default:
                return "unknown";
        }
    }
};

/**
 * @brief A Lego Dimensions vehicle / gadget tag.
 *
 * Page Layout:
 *   Page 23 (offset 0x8C) : upgradesP23  - uint32 LE
 *   Page 24 (offset 0x90) : id           - uint16 LE
 *   Page 25 (offset 0x94) : upgradesP25  - uint32 LE
 *   Page 26 (offset 0x98) : 0x0001       - uint16 BE (verification)
 * For some reason, these pages can also get offset by 0x12 when being written *shrug*
 */
class VehicleTag : public ToyTag {
   public:
    uint32_t upgradesP23;
    uint32_t upgradesP25;

    // Page offsets (byte addresses inside the 180-byte buffer)
    static const uint8_t OFFSET_UPGRADES_P23 = 0x23 * 4;  // page 23 * 4
    static const uint8_t OFFSET_ID = 0x24 * 4;            // page 24 * 4
    static const uint8_t OFFSET_UPGRADES_P25 = 0x25 * 4;  // page 25 * 4
    static const uint8_t OFFSET_VERIFY = 0x26 * 4;        // page 26 * 4

    VehicleTag() : ToyTag(), upgradesP23(0), upgradesP25(0) { type = ToyTagType::Vehicle; }

    VehicleTag(uint16_t vehicleId, const char* vehicleName, uint32_t upg23 = 0, uint32_t upg25 = 0)
        : ToyTag(), upgradesP23(upg23), upgradesP25(upg25) {
        type = ToyTagType::Vehicle;
        id = vehicleId;
        strlcpy(name, vehicleName, sizeof(name));
        commitToBuffer();
    }

    // ------------------------------------------------------------------
    // commitToBuffer() - writes all vehicle data into the NFC byte buffer.
    //                    Call after changing id / upgradesP23 / upgradesP25.
    // ------------------------------------------------------------------

    /**
     * @brief Commits all of the stored vehicle data (upgrades/ID + verification) to the underlying
     * NFC byte buffer
     *
     */
    void commitToBuffer() {
        // Page 23 - upgrade part 1 (uint32 LE)
        Reader::writeUInt32LE(_buf, OFFSET_UPGRADES_P23, upgradesP23);

        // Page 24 - vehicle ID (uint16 LE)
        Reader::writeUInt16LE(_buf, OFFSET_ID, id);

        // Page 25 - upgrade part 2 (uint32 LE)
        Reader::writeUInt32LE(_buf, OFFSET_UPGRADES_P25, upgradesP25);

        // Page 26 - Verification uint16 0x0001
        Reader::writeUInt16BE(_buf, OFFSET_VERIFY, 0x0001);
    }

    /**
     * @brief Parses the underlying NFC buffer in order to fill the Upgrade/ID fields (also checks
     * the verification byte)
     *
     */
    void readFromBuffer() {
        upgradesP23 = Reader::readUInt32LE(_buf, OFFSET_UPGRADES_P23);
        id = Reader::readUInt16LE(_buf, OFFSET_ID);
        upgradesP25 = Reader::readUInt32LE(_buf, OFFSET_UPGRADES_P25);

        const uint16_t verificationByte = Reader::readUInt16BE(_buf, OFFSET_VERIFY);
        // TODO: Check the verification byte
    }

   protected:
    void fillJSON(JsonDocument& doc) const override {
        ToyTag::fillJSON(doc);
        doc["vehicleUpgradesP23"] = upgradesP23;
        doc["vehicleUpgradesP25"] = upgradesP25;
    }

    void parseJSON(const JsonVariant doc) override {
        ToyTag::parseJSON(doc);
        upgradesP23 = doc["vehicleUpgradesP23"] | (uint32_t)0;
        upgradesP25 = doc["vehicleUpgradesP25"] | (uint32_t)0;

        // Restore the byte buffer to match the loaded values
        commitToBuffer();
    }
};

/**
 * @brief A Lego Dimensions character tag.
 *
 * The character ID is handled slightly differently from the READ/normal NFC bytes, by the game via
 * CMD_MODEL command, and not written into the NFC byte pages directly. The raw buffer stays mostly
 * zeroed - the game basically only checks that page 26 is 0x00.
 */
class CharacterTag : public ToyTag {
   public:
    CharacterTag() : ToyTag() { type = ToyTagType::Character; }

    CharacterTag(uint16_t charId, const char* charName) : ToyTag() {
        type = ToyTagType::Character;
        id = charId;
        strlcpy(name, charName, sizeof(name));
        // Page 26 must be 0x00 for character verification - already zero
        // from NFCTag's memset, but we assert it explicitly for clarity.
        Reader::writeUInt32BE(_buf, 0x98, 0x000000);
    }

   protected:
    void fillJSON(JsonDocument& doc) const override {
        ToyTag::fillJSON(doc);
        doc["vehicleUpgradesP23"] = 0;
        doc["vehicleUpgradesP25"] = 0;
    }

    void parseJSON(const JsonVariant doc) override {
        ToyTag::parseJSON(doc);
        // Ignore the upgrade fields for characters
    }
};