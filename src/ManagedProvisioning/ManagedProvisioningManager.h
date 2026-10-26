#pragma once

#include "ManagedProvisioningTypes.h"
#include "../Configuration/DeviceConfiguration.h"
#include "../DeviceProvisioning/DeviceProvisioningManager.h"
#include "../CredentialProvisioning/MqttCredentialProvisioningManager.h"
#include "../Identity/DeviceIdentity.h"
#include "../Provisioning/ProvisioningManager.h"

#include <string>
#include <deque>

enum class ManagedProvisioningState
{
    Idle,
    TestingWifi,
    TestingWifiRecovery,
    WaitingForNetwork,
    RegisteringClaim,
    PollingClaim,
    WaitingForCredentialRequest,
    RequestingCredentials,
    PollingCredentials,
    TestingMqtt,
    Completed,
    Failed
};

class ManagedProvisioningManager
{
public:
    ManagedProvisioningManager(ProvisioningManager &wifiManager,
                               DeviceProvisioningManager &claimManager,
                               MqttCredentialProvisioningManager &credentialManager);

    bool Begin(const DeviceIdentity &identity, const std::string &firmwareVersion,
               const DeviceConfiguration &configuration);
    bool ProcessSerialMessage(const std::string &input, unsigned long now,
                              std::string &response);
    void Loop(unsigned long now);
    void UpdateConfiguration(const DeviceConfiguration &configuration);
    bool TryTakeEvent(std::string &event);
    bool IsWorkflowActive() const;
    void NotifyMqttCredentialResult(bool success, uint32_t credentialVersion,
                                    const char *failureCode = "mqtt_connection_failed");

private:
    static constexpr unsigned long REQUEST_TIMEOUT_MS = 3500;
    static constexpr unsigned long FIRST_RETRY_MS = 2000;
    static constexpr unsigned long MAX_RETRY_MS = 30000;

    ProvisioningManager &wifiManager;
    DeviceProvisioningManager &claimManager;
    MqttCredentialProvisioningManager &credentialManager;
    DeviceIdentity identity;
    DeviceConfiguration activeConfiguration;
    std::string firmwareVersion;
    ManagedBootstrapRequest pendingBootstrap;
    ManagedProvisioningRecord persistedRecord;
    ManagedProvisioningState state;
    std::deque<std::string> pendingEvents;
    unsigned long nextAttemptAt;
    unsigned long retryDelayMs;
    bool initialized;

    bool LoadRecord(ManagedProvisioningRecord &record) const;
    bool SaveRecord(const ManagedProvisioningRecord &record) const;
    bool HandleIdentify(const std::string &input, std::string &response) const;
    bool HandleBootstrap(const std::string &input, unsigned long now, std::string &response);
    bool HandleWifiRecovery(const std::string &input, unsigned long now,
                            std::string &response);
    void ContinueAfterWifi(unsigned long now);
    void RegisterClaim(unsigned long now);
    void PollClaim(unsigned long now);
    void ContinueAfterClaim(unsigned long now);
    void RequestCredentials(unsigned long now);
    void PollCredentials(unsigned long now);
    bool SendHttpRequest(const char *method, const std::string &url,
                         const std::string &body, int &statusCode,
                         std::string &responseBody, std::string &errorCode) const;
    bool ProcessClaimResponse(const std::string &body, unsigned long now);
    CredentialEnvelopeResult ProcessCredentialResponse(const std::string &body);
    void ScheduleRetry(unsigned long now, const char *stage, const char *errorCode);
    void CompleteClaim(unsigned long now);
    void CompleteWorkflow(uint32_t credentialVersion);
    void Fail(const char *stage, const char *errorCode, bool definitive = true);
    bool ConsumeToken(bool completed);
    void Emit(const std::string &event);
};

