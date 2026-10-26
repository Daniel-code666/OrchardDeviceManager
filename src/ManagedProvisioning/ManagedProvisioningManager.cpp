#include "ManagedProvisioningManager.h"

#include "ManagedProvisioningJson.h"
#include "../DeviceProvisioning/ProvisioningJson.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

namespace
{
    constexpr const char *NAMESPACE = "huerta_web";
    constexpr const char *KEY_COMMITTED = "committed";
    constexpr const char *KEY_VERSION = "version";
    constexpr const char *KEY_REQUEST = "request";
    constexpr const char *KEY_BASE_URL = "base_url";
    constexpr const char *KEY_TOKEN = "token";

    std::string ReadString(Preferences &preferences, const char *key)
    {
        const String value = preferences.getString(key, "");
        return std::string(value.c_str(), value.length());
    }

    bool WriteString(Preferences &preferences, const char *key, const std::string &value)
    {
        return !value.empty() && preferences.putString(key, value.c_str()) == value.size();
    }

    bool HasElapsed(unsigned long now, unsigned long target)
    {
        return static_cast<long>(now - target) >= 0;
    }
}

ManagedProvisioningManager::ManagedProvisioningManager(ProvisioningManager &provisioning,
                                                       DeviceProvisioningManager &claims)
    : wifiManager(provisioning), claimManager(claims), state(ManagedProvisioningState::Idle),
      nextAttemptAt(0), retryDelayMs(FIRST_RETRY_MS), initialized(false)
{
}

bool ManagedProvisioningManager::Begin(const DeviceIdentity &newIdentity,
                                       const std::string &newFirmwareVersion,
                                       const DeviceConfiguration &configuration)
{
    identity = newIdentity;
    firmwareVersion = newFirmwareVersion;
    activeConfiguration = configuration;
    initialized = !identity.deviceId.empty() && !firmwareVersion.empty();
    if (!initialized)
        return false;

    ManagedProvisioningRecord loaded;
    if (LoadRecord(loaded))
    {
        persistedRecord = std::move(loaded);
        state = claimManager.IsProvisioned() ? ManagedProvisioningState::Completed
                                             : ManagedProvisioningState::WaitingForNetwork;
    }
    return true;
}

bool ManagedProvisioningManager::ProcessSerialMessage(const std::string &input,
                                                       unsigned long now,
                                                       std::string &response)
{
    std::string type;
    if (!ManagedProvisioningJson::TryGetMessageType(input, type))
    {
        if (input.find("device_bootstrap") != std::string::npos)
        {
            response = ManagedProvisioningJson::SerializeStatus(
                "unknown", "bootstrap", "failed", "invalid_contract");
            return true;
        }
        return false;
    }
    if (type == "device_identify")
        return HandleIdentify(input, response);
    if (type == "device_bootstrap")
        return HandleBootstrap(input, now, response);
    return false;
}

bool ManagedProvisioningManager::HandleIdentify(const std::string &input,
                                                 std::string &response) const
{
    JsonDocument document;
    if (!initialized || deserializeJson(document, input) != DeserializationError::Ok ||
        document["contractVersion"] != 1)
        return false;
    const char *requestId = document["requestId"];
    if (requestId == nullptr || !IsValidProvisioningIdentifier(requestId, 64))
        return false;

    response = ManagedProvisioningJson::SerializeIdentifyResult(
        requestId, identity, firmwareVersion, claimManager.GetState(), activeConfiguration);
    return true;
}

bool ManagedProvisioningManager::HandleBootstrap(const std::string &input,
                                                  unsigned long now,
                                                  std::string &response)
{
    ManagedBootstrapRequest request;
    if (!initialized || !ManagedProvisioningJson::DeserializeBootstrap(input, request))
    {
        response = ManagedProvisioningJson::SerializeStatus("unknown", "bootstrap", "failed",
                                                            "invalid_contract");
        return true;
    }

    if (claimManager.IsProvisioned())
    {
        response = ManagedProvisioningJson::SerializeStatus(
            request.requestId, "bootstrap", "failed", "already_provisioned");
        return true;
    }
    if (IsWorkflowActive())
    {
        response = ManagedProvisioningJson::SerializeStatus(
            request.requestId, "bootstrap", "failed", "workflow_busy");
        return true;
    }

    pendingBootstrap = std::move(request);
    if (!wifiManager.StartManagedWifiConfiguration(pendingBootstrap.wifiSsid,
                                                    pendingBootstrap.wifiPassword))
    {
        response = ManagedProvisioningJson::SerializeStatus(
            pendingBootstrap.requestId, "wifi", "failed", "wifi_test_not_started");
        pendingBootstrap = ManagedBootstrapRequest{};
        return true;
    }

    state = ManagedProvisioningState::TestingWifi;
    nextAttemptAt = now;
    retryDelayMs = FIRST_RETRY_MS;
    response = ManagedProvisioningJson::SerializeStatus(
        pendingBootstrap.requestId, "wifi", "in_progress");
    return true;
}

void ManagedProvisioningManager::Loop(unsigned long now)
{
    if (!initialized)
        return;

    if (state == ManagedProvisioningState::TestingWifi)
    {
        bool success = false;
        std::string detail;
        if (!wifiManager.TryTakeCredentialTestResult(success, detail))
            return;
        if (!success)
        {
            Fail("wifi", detail.c_str());
            return;
        }

        ManagedProvisioningRecord record;
        record.bootstrapRequestId = pendingBootstrap.requestId;
        record.serviceBaseUrl = pendingBootstrap.serviceBaseUrl;
        record.provisioningToken = pendingBootstrap.provisioningToken;
        if (!SaveRecord(record))
        {
            Fail("bootstrap", "storage_error");
            return;
        }

        persistedRecord = std::move(record);
        pendingBootstrap = ManagedBootstrapRequest{};
        Emit(ManagedProvisioningJson::SerializeStatus(
            persistedRecord.bootstrapRequestId, "wifi", "completed"));
        state = ManagedProvisioningState::WaitingForNetwork;
        nextAttemptAt = now;
    }

    if (state == ManagedProvisioningState::WaitingForNetwork)
        ContinueAfterWifi(now);
    else if (state == ManagedProvisioningState::RegisteringClaim && HasElapsed(now, nextAttemptAt))
        RegisterClaim(now);
    else if (state == ManagedProvisioningState::PollingClaim && HasElapsed(now, nextAttemptAt))
        PollClaim(now);
}

void ManagedProvisioningManager::ContinueAfterWifi(unsigned long now)
{
    if (WiFi.status() != WL_CONNECTED)
        return;
    if (claimManager.IsProvisioned())
    {
        Complete();
        return;
    }
    if (claimManager.GetState() == DeviceProvisioningState::Unprovisioned &&
        !claimManager.StartClaim(now))
    {
        Fail("claim", "claim_not_started");
        return;
    }

    state = ManagedProvisioningState::RegisteringClaim;
    nextAttemptAt = now;
    Emit(ManagedProvisioningJson::SerializeStatus(
        persistedRecord.bootstrapRequestId, "claim", "in_progress"));
}

void ManagedProvisioningManager::RegisterClaim(unsigned long now)
{
    ClaimRequest claim;
    if (!claimManager.TryGetPendingRequest(claim))
    {
        if (claimManager.IsProvisioned())
            Complete();
        else
            Fail("claim", "claim_expired");
        return;
    }

    int statusCode = 0;
    std::string responseBody;
    std::string errorCode;
    const std::string url = persistedRecord.serviceBaseUrl + "/api/device-provisioning/claims";
    const std::string body = ManagedProvisioningJson::SerializeClaimRegistration(
        claim, identity, firmwareVersion);
    if (!SendHttpRequest("POST", url, body, statusCode, responseBody, errorCode))
    {
        ScheduleRetry(now, "claim", errorCode.c_str());
        return;
    }
    if (statusCode == 401 || statusCode == 403)
    {
        Fail("claim", "invalid_provisioning_token");
        return;
    }
    if (statusCode == 200)
    {
        if (!ProcessClaimResponse(responseBody, now))
            Fail("claim", "invalid_service_response");
        return;
    }
    if (statusCode == 202)
    {
        state = ManagedProvisioningState::PollingClaim;
        retryDelayMs = FIRST_RETRY_MS;
        nextAttemptAt = now + retryDelayMs;
        return;
    }
    if (statusCode >= 400 && statusCode < 500 && statusCode != 429)
    {
        Fail("claim", "service_rejected_request");
        return;
    }
    ScheduleRetry(now, "claim", "service_unavailable");
}

void ManagedProvisioningManager::PollClaim(unsigned long now)
{
    ClaimRequest claim;
    if (!claimManager.TryGetPendingRequest(claim))
    {
        if (claimManager.IsProvisioned())
            Complete();
        else
            Fail("claim", "claim_expired");
        return;
    }

    int statusCode = 0;
    std::string body;
    std::string errorCode;
    const std::string url = persistedRecord.serviceBaseUrl +
                            "/api/device-provisioning/claims/" + claim.requestId;
    if (!SendHttpRequest("GET", url, "", statusCode, body, errorCode))
    {
        ScheduleRetry(now, "claim", errorCode.c_str());
        return;
    }
    if (statusCode == 200)
    {
        if (!ProcessClaimResponse(body, now))
            Fail("claim", "invalid_service_response");
        return;
    }
    if (statusCode == 401 || statusCode == 403)
    {
        Fail("claim", "invalid_provisioning_token");
        return;
    }
    if (statusCode >= 400 && statusCode < 500 && statusCode != 404 && statusCode != 429)
    {
        Fail("claim", "service_rejected_request");
        return;
    }
    if (statusCode != 202 && statusCode != 404)
    {
        ScheduleRetry(now, "claim", "service_unavailable");
        return;
    }

    retryDelayMs = retryDelayMs >= MAX_RETRY_MS / 2 ? MAX_RETRY_MS : retryDelayMs * 2;
    nextAttemptAt = now + retryDelayMs;
}

bool ManagedProvisioningManager::SendHttpRequest(const char *method,
                                                 const std::string &url,
                                                 const std::string &body,
                                                 int &statusCode,
                                                 std::string &responseBody,
                                                 std::string &errorCode) const
{
    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    // HTTPClient conserva una referencia al transporte y debe destruirse primero.
    HTTPClient http;
    http.setConnectTimeout(REQUEST_TIMEOUT_MS);
    http.setTimeout(REQUEST_TIMEOUT_MS);

    bool begun = false;
    if (url.rfind("https://", 0) == 0)
    {
#if ENABLE_LOCAL_PROVISIONING_MOCK
        // Exclusivo del mock local. La fase de endurecimiento instalará la CA de producción.
        secureClient.setInsecure();
        begun = http.begin(secureClient, url.c_str());
#else
        errorCode = "tls_ca_not_configured";
        return false;
#endif
    }
    else
    {
#if ENABLE_LOCAL_PROVISIONING_MOCK
        begun = http.begin(plainClient, url.c_str());
#else
        errorCode = "https_required";
        return false;
#endif
    }

    if (!begun)
    {
        errorCode = "invalid_service_url";
        return false;
    }

    http.addHeader("Authorization", ("Bearer " + persistedRecord.provisioningToken).c_str());
    http.addHeader("Accept", "application/json");
    if (std::string(method) == "GET")
        statusCode = http.GET();
    else
    {
        http.addHeader("Content-Type", "application/json");
        statusCode = http.POST(String(body.c_str()));
    }

    if (statusCode > 0)
    {
        const String payload = http.getString();
        if (payload.length() <= 4096)
            responseBody.assign(payload.c_str(), payload.length());
        else
        {
            http.end();
            errorCode = "response_too_large";
            return false;
        }
    }
    http.end();

    if (statusCode <= 0)
    {
        errorCode = "network_error";
        return false;
    }
    return true;
}

bool ManagedProvisioningManager::ProcessClaimResponse(const std::string &body,
                                                      unsigned long now)
{
    ClaimResponse response;
    if (!ProvisioningJson::DeserializeResponse(body, response))
        return false;

    const ClaimProcessingResult result = claimManager.ProcessResponse(response, now);
    if (result == ClaimProcessingResult::Approved)
    {
        Complete();
        return true;
    }
    if (result == ClaimProcessingResult::Rejected || result == ClaimProcessingResult::Expired)
    {
        Fail("claim", ToString(result));
        return true;
    }
    return false;
}

void ManagedProvisioningManager::ScheduleRetry(unsigned long now, const char *stage,
                                               const char *errorCode)
{
    Emit(ManagedProvisioningJson::SerializeStatus(
        persistedRecord.bootstrapRequestId, stage, "retrying", errorCode));
    retryDelayMs = retryDelayMs >= MAX_RETRY_MS / 2 ? MAX_RETRY_MS : retryDelayMs * 2;
    nextAttemptAt = now + retryDelayMs;
}

void ManagedProvisioningManager::Complete()
{
    state = ManagedProvisioningState::Completed;
    Emit(ManagedProvisioningJson::SerializeStatus(
        persistedRecord.bootstrapRequestId, "claim", "completed"));
}

void ManagedProvisioningManager::Fail(const char *stage, const char *errorCode)
{
    state = ManagedProvisioningState::Failed;
    const std::string &requestId = pendingBootstrap.requestId.empty()
                                       ? persistedRecord.bootstrapRequestId
                                       : pendingBootstrap.requestId;
    Emit(ManagedProvisioningJson::SerializeStatus(requestId, stage, "failed", errorCode));
    pendingBootstrap = ManagedBootstrapRequest{};
}

void ManagedProvisioningManager::Emit(const std::string &event)
{
    if (pendingEvents.size() < 8)
        pendingEvents.push_back(event);
}

bool ManagedProvisioningManager::TryTakeEvent(std::string &event)
{
    if (pendingEvents.empty())
        return false;
    event = std::move(pendingEvents.front());
    pendingEvents.pop_front();
    return true;
}

void ManagedProvisioningManager::UpdateConfiguration(const DeviceConfiguration &configuration)
{
    activeConfiguration = configuration;
}

bool ManagedProvisioningManager::IsWorkflowActive() const
{
    return state == ManagedProvisioningState::TestingWifi ||
           state == ManagedProvisioningState::WaitingForNetwork ||
           state == ManagedProvisioningState::RegisteringClaim ||
           state == ManagedProvisioningState::PollingClaim;
}

bool ManagedProvisioningManager::LoadRecord(ManagedProvisioningRecord &record) const
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true))
        return false;
    if (!preferences.getBool(KEY_COMMITTED, false))
    {
        preferences.end();
        return false;
    }

    ManagedProvisioningRecord loaded;
    loaded.version = preferences.getUShort(KEY_VERSION, 0);
    loaded.bootstrapRequestId = ReadString(preferences, KEY_REQUEST);
    loaded.serviceBaseUrl = ReadString(preferences, KEY_BASE_URL);
    loaded.provisioningToken = ReadString(preferences, KEY_TOKEN);
    preferences.end();
    if (!loaded.IsValid())
        return false;
    record = std::move(loaded);
    return true;
}

bool ManagedProvisioningManager::SaveRecord(const ManagedProvisioningRecord &record) const
{
    if (!record.IsValid())
        return false;
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false))
        return false;
    bool success = preferences.putBool(KEY_COMMITTED, false) > 0;
    success = preferences.putUShort(KEY_VERSION, record.version) > 0 && success;
    success = WriteString(preferences, KEY_REQUEST, record.bootstrapRequestId) && success;
    success = WriteString(preferences, KEY_BASE_URL, record.serviceBaseUrl) && success;
    success = WriteString(preferences, KEY_TOKEN, record.provisioningToken) && success;
    if (success)
        success = preferences.putBool(KEY_COMMITTED, true) > 0;
    preferences.end();
    if (!success)
        return false;

    ManagedProvisioningRecord persisted;
    return LoadRecord(persisted) &&
           persisted.bootstrapRequestId == record.bootstrapRequestId &&
           persisted.serviceBaseUrl == record.serviceBaseUrl &&
           persisted.provisioningToken == record.provisioningToken;
}

