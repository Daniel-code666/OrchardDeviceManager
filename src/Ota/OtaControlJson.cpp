#include "OtaControlJson.h"

#include <ArduinoJson.h>

bool OtaControlJson::TryParseUpdateRequest(const std::string &json, std::string &manifestUrl)
{
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok ||
        document["type"] != "ota_update_request")
        return false;
    const char *url = document["manifestUrl"];
    if (url == nullptr)
        return false;
    manifestUrl = url;
    return !manifestUrl.empty() && manifestUrl.size() <= 512;
}

