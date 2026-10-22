#include "OtaManifestJson.h"

#include <ArduinoJson.h>

bool OtaManifestJson::Deserialize(const std::string &json, FirmwareManifest &manifest)
{
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok ||
        document["type"] != "firmware_manifest")
        return false;

    const char *releaseId = document["releaseId"];
    const char *version = document["version"];
    const char *board = document["board"];
    const char *sha256 = document["sha256"];
    const char *firmwareUrl = document["firmwareUrl"];
    const char *signature = document["signature"];
    if (releaseId == nullptr || version == nullptr || board == nullptr || sha256 == nullptr ||
        firmwareUrl == nullptr || signature == nullptr)
        return false;

    FirmwareManifest parsed;
    parsed.contractVersion = document["contractVersion"] | 0;
    parsed.releaseId = releaseId;
    parsed.version = version;
    parsed.board = board;
    parsed.size = document["size"] | 0;
    parsed.sha256 = sha256;
    parsed.firmwareUrl = firmwareUrl;
    parsed.signatureHex = signature;
    manifest = std::move(parsed);
    return true;
}

