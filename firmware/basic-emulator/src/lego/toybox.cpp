#include "lego/toybox.h"
#include "log/logger.h"

Toybox::Toybox() : toyCount(0) {}

bool Toybox::addToy(const char *name, const int id, const char *type)
{
    // Check if we have space for another toy
    if (toyCount >= MAX_TOYS)
    {
        return false;
    }

    // Validate input parameters
    if (!name || !type || strlen(name) == 0 || strlen(type) == 0)
    {
        return false;
    }

    // Create the toy object
    JsonDocument &toy = toys[toyCount];
    toy["name"] = name;
    toy["id"] = id;
    toy["type"] = type;

    // Standard filler-data
    toy["index"] = "-1";
    toy["vehicleUpgradesP23"] = 0;
    toy["vehicleUpgradesP25"] = 0;
    toy["uid"] = this->generateUID();

    toyCount++;
    return true;
}

bool Toybox::removeToy(const char *uid)
{
    int index = -1;

    for (int i = 0; i < toyCount; i++)
    {
        if (strcmp(toys[i]["uid"], uid) == 0)
        {
            index = i;
            break;
        }
    }

    // Check if index is valid
    if (!isValidIndex(index))
    {
        return false;
    }

    // Shift all toys after the removed index down by one position
    for (size_t i = index; i < toyCount - 1; i++)
    {
        toys[i] = toys[i + 1];
    }

    // Clear the last toy and decrement count
    toys[toyCount - 1].clear();
    toyCount--;
    return true;
}

JsonDocument *Toybox::getToy(size_t index)
{
    return &toys[index];
}

JsonDocument *Toybox::getByUID(const char *uid)
{
    int index = -1;

    for (int i = 0; i < toyCount; i++)
    {
        if (strcmp(toys[i]["uid"], uid) == 0)
        {
            index = i;
            break;
        }
    }

    // Check if index is valid
    if (!isValidIndex(index))
    {
        return nullptr;
    }

    return &toys[index];
}

String Toybox::serialize() const
{
    // Create a JSON document to hold all toys
    JsonDocument doc;
    JsonArray toyArray = doc.to<JsonArray>();

    // Add each toy to the array
    for (size_t i = 0; i < toyCount; i++)
    {
        JsonObject toy = toyArray.add<JsonObject>();
        toy.set(toys[i].as<JsonObjectConst>());
    }

    // Serialize to string
    String result;
    serializeJson(doc, result);
    return result;
}

bool Toybox::deserialize(const char *json)
{
    // Check for valid input
    if (!json || strlen(json) == 0)
    {
        return false;
    }

    // Create temporary document for parsing
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    // Check for parsing errors
    if (error)
    {
        return false;
    }

    // Check if the JSON has the expected structure
    if (!doc.is<JsonArray>())
    {
        return false;
    }

    JsonArray toyArray = doc.as<JsonArray>();

    // Check if we can fit all the toys
    if (toyArray.size() > MAX_TOYS)
    {
        return false;
    }

    // Clear existing toys
    clear();

    // Load toys from JSON
    for (JsonVariant toy : toyArray)
    {
        if (toyCount >= MAX_TOYS)
        {
            break; // Safety check to prevent overflow
        }

        JsonDocument &newToy = toys[toyCount];

        // Copy all fields directly, using defaults if missing
        newToy.set(toy.as<JsonObjectConst>());
        toyCount++;
    }

    return true;
}

size_t Toybox::count() const
{
    return toyCount;
}

void Toybox::clear()
{
    // Clear all toy documents
    for (size_t i = 0; i < toyCount; i++)
    {
        toys[i].clear();
    }
    toyCount = 0;
}

bool Toybox::isValidIndex(size_t index) const
{
    return index >= 0 && index < toyCount;
}