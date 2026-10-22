#include "DeviceProvisioningManager.h"

#include "ProvisioningAuthentication.h"

#include <esp_random.h>

namespace
{
    constexpr char CLAIM_ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
}

DeviceProvisioningManager::DeviceProvisioningManager(ConfigurationManager &manager)
    : configurationManager(manager), pendingStartedAt(0), stateChangePending(false)
{
}

bool DeviceProvisioningManager::Begin(const std::string &stableDeviceId, unsigned long now)
{
    if (!IsValidProvisioningIdentifier(stableDeviceId))
        return false;

    deviceId = stableDeviceId;
    ProvisioningRecord loaded;
    const ProvisioningLoadResult result = configurationManager.LoadProvisioning(loaded);

    if (result == ProvisioningLoadResult::Loaded)
    {
        record = std::move(loaded);
        if (record.state == DeviceProvisioningState::PendingClaim)
            pendingStartedAt = now;
    }
    else if (result == ProvisioningLoadResult::NotFound)
    {
        record = ProvisioningRecord{};
    }
    else
    {
        if (!configurationManager.ClearProvisioning())
            return false;
        record = ProvisioningRecord{};
    }

    stateChangePending = true;
    return true;
}

bool DeviceProvisioningManager::StartClaim(unsigned long now)
{
    if (record.state != DeviceProvisioningState::Unprovisioned || deviceId.empty())
        return false;

    ProvisioningRecord candidate;
    candidate.state = DeviceProvisioningState::PendingClaim;
    candidate.requestId = GenerateRandomHex(16);
    candidate.claimCode = GenerateClaimCode();

    if (!PersistState(candidate))
        return false;

    pendingStartedAt = now;
    MarkStateChanged();
    return true;
}

void DeviceProvisioningManager::Loop(unsigned long now)
{
    if (record.state == DeviceProvisioningState::PendingClaim &&
        HasElapsed(pendingStartedAt, CLAIM_LIFETIME_MS, now))
    {
        TransitionToUnprovisioned();
    }
}

ClaimProcessingResult DeviceProvisioningManager::ProcessResponse(const ClaimResponse &response, unsigned long now)
{
    Loop(now);
    if (record.state != DeviceProvisioningState::PendingClaim)
        return ClaimProcessingResult::InvalidState;

    if (response.contractVersion != ClaimResponse::CONTRACT_VERSION ||
        response.deviceId != deviceId ||
        !IsValidProvisioningIdentifier(response.deviceId) ||
        !IsValidProvisioningIdentifier(response.requestId, 64) ||
        response.errorCode.size() > 64)
        return ClaimProcessingResult::InvalidContract;

    if (response.requestId != record.requestId)
        return ClaimProcessingResult::RequestMismatch;

    if (!ProvisioningAuthentication::Verify(response, record.claimCode))
        return ClaimProcessingResult::AuthenticationFailed;

    if (response.decision == ClaimDecision::Approved)
    {
        if (!IsValidProvisioningIdentifier(response.userId) ||
            !IsValidProvisioningIdentifier(response.gardenId) ||
            !response.errorCode.empty())
            return ClaimProcessingResult::InvalidContract;

        ProvisioningRecord candidate;
        candidate.state = DeviceProvisioningState::Provisioned;
        candidate.requestId = record.requestId;
        candidate.userId = response.userId;
        candidate.gardenId = response.gardenId;

        if (!PersistState(candidate))
            return ClaimProcessingResult::StorageError;

        MarkStateChanged();
        return ClaimProcessingResult::Approved;
    }

    if (!response.userId.empty() || !response.gardenId.empty() || response.errorCode.empty())
        return ClaimProcessingResult::InvalidContract;

    const ClaimProcessingResult result = response.decision == ClaimDecision::Expired
                                             ? ClaimProcessingResult::Expired
                                             : ClaimProcessingResult::Rejected;
    return TransitionToUnprovisioned() ? result : ClaimProcessingResult::StorageError;
}

bool DeviceProvisioningManager::AuthorizeUnlink()
{
    if (record.state == DeviceProvisioningState::Unprovisioned)
        return true;
    if (record.state != DeviceProvisioningState::Provisioned)
        return false;
    return TransitionToUnprovisioned();
}

DeviceProvisioningState DeviceProvisioningManager::GetState() const
{
    return record.state;
}

bool DeviceProvisioningManager::IsProvisioned() const
{
    return record.state == DeviceProvisioningState::Provisioned;
}

bool DeviceProvisioningManager::TryGetPendingRequest(ClaimRequest &request) const
{
    if (record.state != DeviceProvisioningState::PendingClaim)
        return false;

    request.deviceId = deviceId;
    request.requestId = record.requestId;
    request.claimCode = record.claimCode;
    request.expiresInSeconds = CLAIM_LIFETIME_MS / 1000;
    return true;
}

bool DeviceProvisioningManager::TryTakeStateChange(DeviceProvisioningState &newState)
{
    if (!stateChangePending)
        return false;

    newState = record.state;
    stateChangePending = false;
    return true;
}

bool DeviceProvisioningManager::TransitionToUnprovisioned()
{
    ProvisioningRecord candidate;
    if (!PersistState(candidate))
        return false;

    MarkStateChanged();
    return true;
}

bool DeviceProvisioningManager::PersistState(const ProvisioningRecord &candidate)
{
    if (!candidate.IsValid() || !configurationManager.SaveProvisioning(candidate))
        return false;

    record = candidate;
    return true;
}

void DeviceProvisioningManager::MarkStateChanged()
{
    stateChangePending = true;
}

std::string DeviceProvisioningManager::GenerateRandomHex(size_t byteCount)
{
    constexpr char HEX[] = "0123456789abcdef";
    std::string value(byteCount * 2, '0');
    for (size_t index = 0; index < byteCount; ++index)
    {
        const uint8_t randomByte = static_cast<uint8_t>(esp_random());
        value[index * 2] = HEX[randomByte >> 4];
        value[index * 2 + 1] = HEX[randomByte & 0x0f];
    }
    return value;
}

std::string DeviceProvisioningManager::GenerateClaimCode()
{
    constexpr size_t CODE_LENGTH = 12;
    std::string code;
    code.reserve(CODE_LENGTH);
    for (size_t index = 0; index < CODE_LENGTH; ++index)
        code.push_back(CLAIM_ALPHABET[esp_random() % (sizeof(CLAIM_ALPHABET) - 1)]);
    return code;
}

bool DeviceProvisioningManager::HasElapsed(unsigned long startedAt, unsigned long duration, unsigned long now)
{
    return static_cast<unsigned long>(now - startedAt) >= duration;
}

