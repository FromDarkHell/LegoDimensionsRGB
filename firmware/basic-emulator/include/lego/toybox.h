#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class Toybox
{
public:
    Toybox();

    bool addToy(const char *name, const int id, const char *type);
    bool removeToy(const char *uid);

    JsonDocument *getToy(size_t index);
    JsonDocument *getByUID(const char *uid);

    String serialize() const;
    bool deserialize(const char *json);

    size_t count() const;

    // Clear all toys
    void clear();

    // Check if a toy exists at the given index
    bool isValidIndex(size_t index) const;

private:
    const String generateUID()
    {
        const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        String uid = "";

        for (int i = 0; i < 14; i++)
        {
            uid += chars[random(0, 36)];
        }

        return uid;
    }

    static const size_t MAX_TOYS = 255;       // Maximum number of toys
    static const size_t JSON_BUFFER_SIZE = 8; // Buffer size for JSON operations

    JsonDocument toys[MAX_TOYS]; // Array to store toy objects
    size_t toyCount;             // Current number of toys
};
