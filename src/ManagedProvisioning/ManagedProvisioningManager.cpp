#include "ManagedProvisioningManager.h"

#include "ManagedProvisioningJson.h"
#include "../DeviceProvisioning/ProvisioningJson.h"
#include "../CredentialProvisioning/CredentialProvisioningJson.h"
#include "../Security/TrustedRootCa.h"

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
    constexpr const char *KEY_TOKEN_CONSUMED = "token_used";
    constexpr const char *KEY_COMPLETED = "completed";

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
                                                       DeviceProvisioningManager &claims,
                                                       MqttCredentialProvisioningManager &credentials)
    : wifiManager(provisioning), claimManager(claims), credentialManager(credentials),
      state(ManagedProvisioningState::Idle),
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
        if (persistedRecord.workflowCompleted)
            state = ManagedProvisioningState::Completed;
        else if (claimManager.IsProvisioned())
            state = ManagedProvisioningState::WaitingForCredentialRequest;
        else
            state = ManagedProvisioningState::WaitingForNetwork;
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
    if (type == "device_wifi_reconfigure")
        return HandleWifiRecovery(input, now, response);
    return false;
}

bool ManagedProvisioningManager::HandleWifiRecovery(const std::string &input,
                                                     unsigned long now,
                                                     std::string &response)
{
    ManagedWifiRecoveryRequest request;
    if (!initialized || !ManagedProvisioningJson::DeserializeWifiRecovery(input, request))
    {
        response = ManagedProvisioningJson::SerializeStatus(
            "unknown", "wifi_recovery", "failed", "invalid_contract");
        return true;
    }
    if (!claimManager.IsProvisioned())
    {
        response = ManagedProvisioningJson::SerializeStatus(
            request.requestId, "wifi_recovery", "failed", "device_not_provisioned");
        return true;
    }
    if (IsWorkflowActive())
    {
        response = ManagedProvisioningJson::SerializeStatus(
            request.requestId, "wifi_recovery", "failed", "workflow_busy");
        return true;
    }
    pendingBootstrap = ManagedBootstrapRequest{};
    pendingBootstrap.requestId = request.requestId;
    if (!wifiManager.StartManagedWifiConfiguration(request.wifiSsid, request.wifiPassword))
    {
        response = ManagedProvisioningJson::SerializeStatus(
            request.requestId, "wifi_recovery", "failed", "wifi_test_not_started");
        return true;
    }
    state = ManagedProvisioningState::TestingWifiRecovery;
    nextAttemptAt = now;
    response = ManagedProvisioningJson::SerializeStatus(
        request.requestId, "wifi_recovery", "in_progress");
    return true;
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

    if (state == ManagedProvisioningState::TestingWifi ||
        state == ManagedProvisioningState::TestingWifiRecovery)
    {
        const bool recoveryOnly = state == ManagedProvisioningState::TestingWifiRecovery;
        bool success = false;
        std::string detail;
        if (!wifiManager.TryTakeCredentialTestResult(success, detail))
            return;
        if (!success)
        {
            if (recoveryOnly)
            {
                Emit(ManagedProvisioningJson::SerializeStatus(
                    pendingBootstrap.requestId, "wifi_recovery", "failed", detail.c_str()));
                pendingBootstrap = ManagedBootstrapRequest{};
                state = persistedRecord.workflowCompleted
                            ? ManagedProvisioningState::Completed
                            : ManagedProvisioningState::Idle;
                return;
            }
            Fail("wifi", detail.c_str());
            return;
        }

        if (recoveryOnly)
        {
            Emit(ManagedProvisioningJson::SerializeStatus(
                pendingBootstrap.requestId, "wifi_recovery", "completed"));
            pendingBootstrap = ManagedBootstrapRequest{};
            state = persistedRecord.workflowCompleted
                        ? ManagedProvisioningState::Completed
                        : ManagedProvisioningState::Idle;
            return;
        }

        ManagedProvisioningRecord record;
        record.bootstrapRequestId = pendingBootstrap.requestId;
        record.serviceBaseUrl = pendingBootstrap.serviceBaseUrl;
        record.provisioningToken = pendingBootstrap.provisioningToken;
        record.tokenConsumed = false;
        record.workflowCompleted = false;
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
    else if (state == ManagedProvisioningState::WaitingForCredentialRequest)
        ContinueAfterClaim(now);
    else if (state == ManagedProvisioningState::RequestingCredentials && HasElapsed(now, nextAttemptAt))
        RequestCredentials(now);
    else if (state == ManagedProvisioningState::PollingCredentials && HasElapsed(now, nextAttemptAt))
        PollCredentials(now);
}

void ManagedProvisioningManager::ContinueAfterWifi(unsigned long now)
{
    if (WiFi.status() != WL_CONNECTED)
        return;
    if (claimManager.IsProvisioned())
    {
        CompleteClaim(now);
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
            CompleteClaim(now);
        else
            Fail("claim", "claim_expired");
        return;
    }

    int statusCode = 0;
    std::string responseBody;
    std::string errorCode;
    const std::string url = persistedRecord.serviceBaseUrl + "/api/DeviceProvisioning/claims";
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
            CompleteClaim(now);
        else
            Fail("claim", "claim_expired");
        return;
    }

    int statusCode = 0;
    std::string body;
    std::string errorCode;
    const std::string url = persistedRecord.serviceBaseUrl +
                            "/api/DeviceProvisioning/claims/" + claim.requestId;
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

void ManagedProvisioningManager::ContinueAfterClaim(unsigned long now)
{
    if (WiFi.status() != WL_CONNECTED || !claimManager.IsProvisioned())
        return;

    MqttCredentialRequest request;
    std::string bootstrapKey;
    if (!credentialManager.TryGetPendingRequest(request))
    {
        if (credentialManager.GetAppliedCredentialVersion() > 0 &&
            activeConfiguration.HasMqttConfiguration())
        {
            state = ManagedProvisioningState::TestingMqtt;
            Emit(ManagedProvisioningJson::SerializeStatus(
                persistedRecord.bootstrapRequestId, "mqtt", "in_progress"));
        }
        return;
    }
    if (!credentialManager.TryGetLocalBootstrapKey(bootstrapKey))
        return;

    state = ManagedProvisioningState::RequestingCredentials;
    retryDelayMs = FIRST_RETRY_MS;
    nextAttemptAt = now;
    Emit(ManagedProvisioningJson::SerializeStatus(
        persistedRecord.bootstrapRequestId, "mqtt_credentials", "in_progress"));
}

void ManagedProvisioningManager::RequestCredentials(unsigned long now)
{
    MqttCredentialRequest request;
    std::string bootstrapKey;
    if (!credentialManager.TryGetPendingRequest(request) ||
        !credentialManager.TryGetLocalBootstrapKey(bootstrapKey))
    {
        Fail("mqtt_credentials", "credential_request_unavailable");
        return;
    }

    int statusCode = 0;
    std::string responseBody;
    std::string errorCode;
    const std::string url = persistedRecord.serviceBaseUrl +
                            "/api/DeviceProvisioning/mqtt-credentials";
    const std::string body = ManagedProvisioningJson::SerializeCredentialRegistration(
        request, bootstrapKey);
    // La clave solo vive en esta solicitud al servicio. Nunca se emite como evento serie.
    bootstrapKey.assign(bootstrapKey.size(), '0');
    bootstrapKey.clear();

    if (!SendHttpRequest("POST", url, body, statusCode, responseBody, errorCode))
    {
        ScheduleRetry(now, "mqtt_credentials", errorCode.c_str());
        return;
    }
    if (statusCode == 401 || statusCode == 403 || statusCode == 410)
    {
        Fail("mqtt_credentials", "invalid_provisioning_token");
        return;
    }
    if (statusCode == 200)
    {
        const CredentialEnvelopeResult result = ProcessCredentialResponse(responseBody);
        if (result != CredentialEnvelopeResult::Accepted)
            Fail("mqtt_credentials", ToString(result));
        return;
    }
    if (statusCode == 202)
    {
        state = ManagedProvisioningState::PollingCredentials;
        retryDelayMs = FIRST_RETRY_MS;
        nextAttemptAt = now + retryDelayMs;
        return;
    }
    if (statusCode >= 400 && statusCode < 500 && statusCode != 429)
    {
        Fail("mqtt_credentials", "service_rejected_request");
        return;
    }
    ScheduleRetry(now, "mqtt_credentials", "service_unavailable");
}

void ManagedProvisioningManager::PollCredentials(unsigned long now)
{
    MqttCredentialRequest request;
    if (!credentialManager.TryGetPendingRequest(request))
    {
        Fail("mqtt_credentials", "credential_request_unavailable");
        return;
    }

    int statusCode = 0;
    std::string body;
    std::string errorCode;
    const std::string url = persistedRecord.serviceBaseUrl +
                            "/api/DeviceProvisioning/mqtt-credentials/" + request.requestId;
    if (!SendHttpRequest("GET", url, "", statusCode, body, errorCode))
    {
        ScheduleRetry(now, "mqtt_credentials", errorCode.c_str());
        return;
    }
    if (statusCode == 200)
    {
        const CredentialEnvelopeResult result = ProcessCredentialResponse(body);
        if (result != CredentialEnvelopeResult::Accepted)
            Fail("mqtt_credentials", ToString(result));
        return;
    }
    if (statusCode == 401 || statusCode == 403 || statusCode == 410)
    {
        Fail("mqtt_credentials", "invalid_provisioning_token");
        return;
    }
    if (statusCode >= 400 && statusCode < 500 && statusCode != 404 && statusCode != 429)
    {
        Fail("mqtt_credentials", "service_rejected_request");
        return;
    }
    retryDelayMs = retryDelayMs >= MAX_RETRY_MS / 2 ? MAX_RETRY_MS : retryDelayMs * 2;
    nextAttemptAt = now + retryDelayMs;
}

CredentialEnvelopeResult ManagedProvisioningManager::ProcessCredentialResponse(
    const std::string &body)
{
    EncryptedMqttCredentials envelope;
    if (!CredentialProvisioningJson::DeserializeEnvelope(body, envelope))
        return CredentialEnvelopeResult::InvalidContract;

    const CredentialEnvelopeResult result = credentialManager.ProcessEnvelope(envelope);
    if (result != CredentialEnvelopeResult::Accepted)
        return result;

    state = ManagedProvisioningState::TestingMqtt;
    Emit(ManagedProvisioningJson::SerializeStatus(
        persistedRecord.bootstrapRequestId, "mqtt", "in_progress"));
    return CredentialEnvelopeResult::Accepted;
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
        if (HUERTA_TRUSTED_ROOT_CA[0] == '\0')
            secureClient.setInsecure();
        else
            secureClient.setCACert(HUERTA_TRUSTED_ROOT_CA);
        begun = http.begin(secureClient, url.c_str());
#else
        if (HUERTA_TRUSTED_ROOT_CA[0] == '\0')
        {
            errorCode = "tls_ca_not_configured";
            return false;
        }
        secureClient.setCACert(HUERTA_TRUSTED_ROOT_CA);
        begun = http.begin(secureClient, url.c_str());
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

    // http.addHeader("Authorization", ("Bearer " + persistedRecord.provisioningToken).c_str());
    http.setAuthorizationType("Bearer");
    http.setAuthorization(persistedRecord.provisioningToken.c_str());
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
        CompleteClaim(now);
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

void ManagedProvisioningManager::CompleteClaim(unsigned long now)
{
    state = ManagedProvisioningState::WaitingForCredentialRequest;
    nextAttemptAt = now;
    Emit(ManagedProvisioningJson::SerializeStatus(
        persistedRecord.bootstrapRequestId, "claim", "completed"));
}

void ManagedProvisioningManager::NotifyMqttCredentialResult(bool success,
                                                             uint32_t credentialVersion,
                                                             const char *failureCode)
{
    if (state != ManagedProvisioningState::TestingMqtt)
        return;
    if (success)
    {
        CompleteWorkflow(credentialVersion);
        return;
    }

    state = ManagedProvisioningState::Failed;
    ConsumeToken(false);
    Emit(ManagedProvisioningJson::SerializeStatus(
        persistedRecord.bootstrapRequestId, "mqtt", "failed", failureCode));
    Emit(ManagedProvisioningJson::SerializeConfigurationResult(
        persistedRecord.bootstrapRequestId, identity, firmwareVersion, false,
        credentialManager.GetAppliedCredentialVersion(), failureCode));
}

void ManagedProvisioningManager::CompleteWorkflow(uint32_t credentialVersion)
{
    if (!ConsumeToken(true))
    {
        Fail("bootstrap", "token_cleanup_failed", false);
        return;
    }
    state = ManagedProvisioningState::Completed;
    Emit(ManagedProvisioningJson::SerializeStatus(
        persistedRecord.bootstrapRequestId, "mqtt", "completed"));
    Emit(ManagedProvisioningJson::SerializeConfigurationResult(
        persistedRecord.bootstrapRequestId, identity, firmwareVersion, true,
        credentialVersion));
}

void ManagedProvisioningManager::Fail(const char *stage, const char *errorCode, bool definitive)
{
    state = ManagedProvisioningState::Failed;
    const std::string &requestId = pendingBootstrap.requestId.empty()
                                       ? persistedRecord.bootstrapRequestId
                                       : pendingBootstrap.requestId;
    Emit(ManagedProvisioningJson::SerializeStatus(requestId, stage, "failed", errorCode));
    if (definitive && !persistedRecord.bootstrapRequestId.empty())
        ConsumeToken(false);
    if (claimManager.IsProvisioned())
        Emit(ManagedProvisioningJson::SerializeConfigurationResult(
            requestId, identity, firmwareVersion, false,
            credentialManager.GetAppliedCredentialVersion(), errorCode));
    pendingBootstrap = ManagedBootstrapRequest{};
}

bool ManagedProvisioningManager::ConsumeToken(bool completed)
{
    if (persistedRecord.bootstrapRequestId.empty())
        return false;
    ManagedProvisioningRecord candidate = persistedRecord;
    candidate.provisioningToken.assign(candidate.provisioningToken.size(), '0');
    candidate.provisioningToken.clear();
    candidate.tokenConsumed = true;
    candidate.workflowCompleted = completed;
    if (!SaveRecord(candidate))
        return false;
    persistedRecord = std::move(candidate);
    return true;
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
           state == ManagedProvisioningState::TestingWifiRecovery ||
           state == ManagedProvisioningState::WaitingForNetwork ||
           state == ManagedProvisioningState::RegisteringClaim ||
           state == ManagedProvisioningState::PollingClaim ||
           state == ManagedProvisioningState::WaitingForCredentialRequest ||
           state == ManagedProvisioningState::RequestingCredentials ||
           state == ManagedProvisioningState::PollingCredentials ||
           state == ManagedProvisioningState::TestingMqtt;
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
    loaded.tokenConsumed = preferences.getBool(KEY_TOKEN_CONSUMED, false);
    loaded.workflowCompleted = preferences.getBool(KEY_COMPLETED, false);
    preferences.end();
    if (loaded.version == 1)
        loaded.version = ManagedProvisioningRecord::CURRENT_VERSION;
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
    if (record.provisioningToken.empty())
    {
        if (preferences.isKey(KEY_TOKEN))
            success = preferences.remove(KEY_TOKEN) && success;
    }
    else
        success = WriteString(preferences, KEY_TOKEN, record.provisioningToken) && success;
    success = preferences.putBool(KEY_TOKEN_CONSUMED, record.tokenConsumed) > 0 && success;
    success = preferences.putBool(KEY_COMPLETED, record.workflowCompleted) > 0 && success;
    if (success)
        success = preferences.putBool(KEY_COMMITTED, true) > 0;
    preferences.end();
    if (!success)
        return false;

    ManagedProvisioningRecord persisted;
    return LoadRecord(persisted) &&
           persisted.bootstrapRequestId == record.bootstrapRequestId &&
           persisted.serviceBaseUrl == record.serviceBaseUrl &&
           persisted.provisioningToken == record.provisioningToken &&
           persisted.tokenConsumed == record.tokenConsumed &&
           persisted.workflowCompleted == record.workflowCompleted;
}

