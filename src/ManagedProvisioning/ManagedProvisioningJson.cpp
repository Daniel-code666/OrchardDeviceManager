#include "ManagedProvisioningJson.h"

#include <ArduinoJson.h>

namespace
{
    std::string ToJson(JsonDocument &document)
    {
        std::string output;
        serializeJson(document, output);
        return output;
    }
}

bool ManagedProvisioningJson::TryGetMessageType(const std::string &json, std::string &type)
{
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok)
        return false;
    const char *value = document["type"];
    if (value == nullptr)
        return false;
    type = value;
    return true;
}

bool ManagedProvisioningJson::DeserializeBootstrap(const std::string &json,
                                                    ManagedBootstrapRequest &request)
{
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok ||
        document["type"] != "device_bootstrap")
        return false;

    const JsonObject root = document.as<JsonObject>();
    const JsonObject wifi = document["wifi"].as<JsonObject>();
    const JsonObject service = document["service"].as<JsonObject>();
    if (root.size() != 6 || wifi.isNull() || wifi.size() != 2 ||
        service.isNull() || service.size() != 1)
        return false;

    const char *requestId = document["requestId"];
    const char *ssid = document["wifi"]["ssid"];
    const char *password = document["wifi"]["password"];
    const char *baseUrl = document["service"]["baseUrl"];
    const char *token = document["provisioningToken"];
    if (requestId == nullptr || ssid == nullptr || password == nullptr ||
        baseUrl == nullptr || token == nullptr)
        return false;

    ManagedBootstrapRequest parsed;
    parsed.contractVersion = document["contractVersion"] | 0;
    parsed.requestId = requestId;
    parsed.wifiSsid = ssid;
    parsed.wifiPassword = password;
    parsed.serviceBaseUrl = baseUrl;
    while (parsed.serviceBaseUrl.size() > 8 && parsed.serviceBaseUrl.back() == '/')
        parsed.serviceBaseUrl.pop_back();
    parsed.provisioningToken = token;
    if (!parsed.IsValid())
        return false;

    request = std::move(parsed);
    return true;
}

bool ManagedProvisioningJson::DeserializeWifiRecovery(
    const std::string &json, ManagedWifiRecoveryRequest &request)
{
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok ||
        document["type"] != "device_wifi_reconfigure")
        return false;
    const JsonObject root = document.as<JsonObject>();
    const JsonObject wifi = document["wifi"].as<JsonObject>();
    if (root.size() != 4 || wifi.isNull() || wifi.size() != 2)
        return false;
    const char *requestId = document["requestId"];
    const char *ssid = document["wifi"]["ssid"];
    const char *password = document["wifi"]["password"];
    if (requestId == nullptr || ssid == nullptr || password == nullptr)
        return false;
    ManagedWifiRecoveryRequest parsed;
    parsed.contractVersion = document["contractVersion"] | 0;
    parsed.requestId = requestId;
    parsed.wifiSsid = ssid;
    parsed.wifiPassword = password;
    if (!parsed.IsValid())
        return false;
    request = std::move(parsed);
    return true;
}

std::string ManagedProvisioningJson::SerializeIdentifyResult(
    const std::string &requestId, const DeviceIdentity &identity,
    const std::string &firmwareVersion, DeviceProvisioningState provisioningState,
    const DeviceConfiguration &configuration)
{
    JsonDocument document;
    document["type"] = "device_identify_result";
    document["contractVersion"] = 1;
    document["requestId"] = requestId;
    document["success"] = true;
    document["deviceId"] = identity.deviceId;
    document["hardwareId"] = identity.hardwareId;
    document["mqttClientId"] = identity.mqttClientId;
    document["board"] = "esp32-c3-devkitm-1";
    document["firmwareVersion"] = firmwareVersion;
    document["provisioningState"] = ToString(provisioningState);
    document["wifiConfigured"] = configuration.HasWifiConfiguration();
    document["mqttConfigured"] = configuration.HasMqttConfiguration();
    return ToJson(document);
}

std::string ManagedProvisioningJson::SerializeStatus(const std::string &requestId,
                                                      const char *stage,
                                                      const char *status,
                                                      const char *errorCode)
{
    JsonDocument document;
    document["type"] = "device_configuration_status";
    document["contractVersion"] = 1;
    document["requestId"] = requestId;
    document["stage"] = stage;
    document["status"] = status;
    if (errorCode != nullptr && errorCode[0] != '\0')
        document["errorCode"] = errorCode;
    return ToJson(document);
}

std::string ManagedProvisioningJson::SerializeClaimRegistration(
    const ClaimRequest &claim, const DeviceIdentity &identity,
    const std::string &firmwareVersion)
{
    JsonDocument document;
    document["type"] = "claim_registration";
    document["contractVersion"] = 1;
    document["deviceId"] = claim.deviceId;
    document["hardwareId"] = identity.hardwareId;
    document["mqttClientId"] = identity.mqttClientId;
    document["board"] = "esp32-c3-devkitm-1";
    document["firmwareVersion"] = firmwareVersion;
    document["requestId"] = claim.requestId;
    document["claimCode"] = claim.claimCode;
    document["expiresInSeconds"] = claim.expiresInSeconds;
    return ToJson(document);
}

std::string ManagedProvisioningJson::SerializeCredentialRegistration(
    const MqttCredentialRequest &request, const std::string &bootstrapKeyHex)
{
    JsonDocument document;
    document["type"] = "mqtt_credentials_registration";
    document["contractVersion"] = 1;
    document["deviceId"] = request.deviceId;
    document["requestId"] = request.requestId;
    document["currentCredentialVersion"] = request.currentCredentialVersion;
    document["bootstrapKey"] = bootstrapKeyHex;
    return ToJson(document);
}

std::string ManagedProvisioningJson::SerializeConfigurationResult(
    const std::string &requestId, const DeviceIdentity &identity,
    const std::string &firmwareVersion, bool success,
    uint32_t credentialVersion, const char *failureCode)
{
    JsonDocument document;
    document["type"] = "device_configuration_result";
    document["contractVersion"] = 1;
    document["requestId"] = requestId;
    document["success"] = success;
    document["deviceId"] = identity.deviceId;
    document["firmwareVersion"] = firmwareVersion;
    document["provisioningState"] = "provisioned";
    document["wifiConnected"] = true;
    document["mqttConnected"] = success;
    document["mqttCredentialVersion"] = credentialVersion;
    if (failureCode != nullptr && failureCode[0] != '\0')
        document["failureCode"] = failureCode;
    else
        document["failureCode"] = nullptr;
    return ToJson(document);
}

