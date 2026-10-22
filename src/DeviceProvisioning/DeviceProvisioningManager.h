#pragma once

#include "ProvisioningTypes.h"
#include "../Configuration/ConfigurationManager.h"

#include <string>

class DeviceProvisioningManager
{
public:
    explicit DeviceProvisioningManager(ConfigurationManager &configurationManager);

    bool Begin(const std::string &deviceId, unsigned long now);
    bool StartClaim(unsigned long now);
    void Loop(unsigned long now);
    ClaimProcessingResult ProcessResponse(const ClaimResponse &response, unsigned long now);

    DeviceProvisioningState GetState() const;
    bool IsProvisioned() const;
    bool TryGetPendingRequest(ClaimRequest &request) const;
    bool TryTakeStateChange(DeviceProvisioningState &newState);

private:
    static constexpr unsigned long CLAIM_LIFETIME_MS = 300000;

    ConfigurationManager &configurationManager;
    ProvisioningRecord record;
    std::string deviceId;
    unsigned long pendingStartedAt;
    bool stateChangePending;

    bool TransitionToUnprovisioned();
    bool PersistState(const ProvisioningRecord &candidate);
    void MarkStateChanged();
    static std::string GenerateRandomHex(size_t byteCount);
    static std::string GenerateClaimCode();
    static bool HasElapsed(unsigned long startedAt, unsigned long duration, unsigned long now);
};

