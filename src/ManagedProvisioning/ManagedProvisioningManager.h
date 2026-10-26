#pragma once

#include "ManagedProvisioningTypes.h"
#include "../Configuration/DeviceConfiguration.h"
#include "../DeviceProvisioning/DeviceProvisioningManager.h"
#include "../Identity/DeviceIdentity.h"
#include "../Provisioning/ProvisioningManager.h"

#include <string>
#include <deque>

enum class ManagedProvisioningState
{
    Idle,
    TestingWifi,
    WaitingForNetwork,
    RegisteringClaim,
    PollingClaim,
    Completed,
    Failed
};

class ManagedProvisioningManager
{
public:
    ManagedProvisioningManager(ProvisioningManager &wifiManager,
                               DeviceProvisioningManager &claimManager);

    bool Begin(const DeviceIdentity &identity, const std::string &firmwareVersion,
               const DeviceConfiguration &configuration);
    bool ProcessSerialMessage(const std::string &input, unsigned long now,
                              std::string &response);
    void Loop(unsigned long now);
    void UpdateConfiguration(const DeviceConfiguration &configuration);
    bool TryTakeEvent(std::string &event);
    bool IsWorkflowActive() const;

private:
    static constexpr unsigned long REQUEST_TIMEOUT_MS = 3500;
    static constexpr unsigned long FIRST_RETRY_MS = 2000;
    static constexpr unsigned long MAX_RETRY_MS = 30000;

    ProvisioningManager &wifiManager;
    DeviceProvisioningManager &claimManager;
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
    void ContinueAfterWifi(unsigned long now);
    void RegisterClaim(unsigned long now);
    void PollClaim(unsigned long now);
    bool SendHttpRequest(const char *method, const std::string &url,
                         const std::string &body, int &statusCode,
                         std::string &responseBody, std::string &errorCode) const;
    bool ProcessClaimResponse(const std::string &body, unsigned long now);
    void ScheduleRetry(unsigned long now, const char *stage, const char *errorCode);
    void Complete();
    void Fail(const char *stage, const char *errorCode);
    void Emit(const std::string &event);
};

