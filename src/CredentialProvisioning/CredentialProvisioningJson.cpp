#include "CredentialProvisioningJson.h"

#include <ArduinoJson.h>

std::string CredentialProvisioningJson::Serialize(const MqttCredentialRequest &request)
{
    JsonDocument document;
    document["type"] = "mqtt_credentials_request";
    document["contractVersion"] = request.contractVersion;
    document["deviceId"] = request.deviceId;
    document["requestId"] = request.requestId;
    document["currentCredentialVersion"] = request.currentCredentialVersion;
    std::string json;
    serializeJson(document, json);
    return json;
}

bool CredentialProvisioningJson::DeserializeEnvelope(const std::string &json,
                                                       EncryptedMqttCredentials &envelope)
{
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok ||
        document["type"] != "mqtt_credentials_envelope")
        return false;

    const char *deviceId = document["deviceId"];
    const char *requestId = document["requestId"];
    const char *nonce = document["nonce"];
    const char *ciphertext = document["ciphertext"];
    const char *authenticationTag = document["authenticationTag"];
    if (deviceId == nullptr || requestId == nullptr || nonce == nullptr ||
        ciphertext == nullptr || authenticationTag == nullptr)
        return false;

    EncryptedMqttCredentials parsed;
    parsed.contractVersion = document["contractVersion"] | 0;
    parsed.deviceId = deviceId;
    parsed.requestId = requestId;
    parsed.nonceHex = nonce;
    parsed.ciphertextHex = ciphertext;
    parsed.authenticationTagHex = authenticationTag;
    envelope = std::move(parsed);
    return true;
}

