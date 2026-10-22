#include "RecoveryJson.h"

#include <ArduinoJson.h>

bool RecoveryJson::DeserializeCommand(const std::string &json, RecoveryCommand &command)
{
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok ||
        document["type"] != "recovery_command")
        return false;

    const char *deviceId = document["deviceId"];
    const char *requestId = document["requestId"];
    const char *action = document["action"];
    const char *authenticationTag = document["authenticationTag"];
    if (deviceId == nullptr || requestId == nullptr || action == nullptr ||
        authenticationTag == nullptr)
        return false;

    RecoveryCommand parsed;
    parsed.contractVersion = document["contractVersion"] | 0;
    parsed.deviceId = deviceId;
    parsed.requestId = requestId;
    parsed.commandVersion = document["commandVersion"] | 0;
    parsed.authenticationTag = authenticationTag;

    const std::string actionValue = action;
    if (actionValue == "revoke_mqtt")
        parsed.action = RecoveryAction::RevokeMqtt;
    else if (actionValue == "unlink")
        parsed.action = RecoveryAction::Unlink;
    else
        return false;

    command = std::move(parsed);
    return true;
}

