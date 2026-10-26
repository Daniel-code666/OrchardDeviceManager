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

bool OtaControlJson::DeserializeRemoteRequest(const std::string &json,
                                              RemoteOtaRequest &request)
{
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok ||
        document["type"] != "ota_update_request")
        return false;
    const JsonObject root = document.as<JsonObject>();
    if (root.size() != 9)
        return false;
    const char *requestId = document["requestId"];
    const char *deviceId = document["deviceId"];
    const char *releaseId = document["releaseId"];
    const char *manifestUrl = document["manifestUrl"];
    const char *expiresAt = document["expiresAt"];
    const char *tag = document["authenticationTag"];
    if (requestId == nullptr || deviceId == nullptr || releaseId == nullptr ||
        manifestUrl == nullptr || expiresAt == nullptr || tag == nullptr)
        return false;
    RemoteOtaRequest parsed;
    parsed.contractVersion = document["contractVersion"] | 0;
    parsed.commandVersion = document["commandVersion"] | 0;
    parsed.requestId = requestId;
    parsed.deviceId = deviceId;
    parsed.releaseId = releaseId;
    parsed.manifestUrl = manifestUrl;
    parsed.expiresAt = expiresAt;
    parsed.authenticationTag = tag;
    request = std::move(parsed);
    return true;
}

