#include "ProvisioningJson.h"

#include <ArduinoJson.h>

std::string ProvisioningJson::Serialize(const ClaimRequest &request)
{
    JsonDocument document;
    document["type"] = "claim_request";
    document["contractVersion"] = request.contractVersion;
    document["deviceId"] = request.deviceId;
    document["requestId"] = request.requestId;
    document["claimCode"] = request.claimCode;
    document["expiresInSeconds"] = request.expiresInSeconds;

    std::string json;
    serializeJson(document, json);
    return json;
}

bool ProvisioningJson::DeserializeResponse(const std::string &json, ClaimResponse &response)
{
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok)
        return false;

    if (document["type"] != "claim_response")
        return false;

    const char *deviceId = document["deviceId"];
    const char *requestId = document["requestId"];
    const char *decision = document["decision"];
    const char *authenticationTag = document["authenticationTag"];
    if (deviceId == nullptr || requestId == nullptr || decision == nullptr || authenticationTag == nullptr)
        return false;

    ClaimResponse parsed;
    parsed.contractVersion = document["contractVersion"] | 0;
    parsed.deviceId = deviceId;
    parsed.requestId = requestId;
    parsed.userId = document["userId"] | "";
    parsed.gardenId = document["gardenId"] | "";
    parsed.errorCode = document["errorCode"] | "";
    parsed.authenticationTag = authenticationTag;

    const std::string decisionValue = decision;
    if (decisionValue == "approved")
        parsed.decision = ClaimDecision::Approved;
    else if (decisionValue == "rejected")
        parsed.decision = ClaimDecision::Rejected;
    else if (decisionValue == "expired")
        parsed.decision = ClaimDecision::Expired;
    else
        return false;

    response = std::move(parsed);
    return true;
}

