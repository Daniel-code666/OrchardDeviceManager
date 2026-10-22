#include "ProvisioningTypes.h"

namespace
{
    bool IsAllowedIdentifierCharacter(char value)
    {
        return (value >= 'a' && value <= 'z') ||
               (value >= 'A' && value <= 'Z') ||
               (value >= '0' && value <= '9') ||
               value == '-' || value == '_' || value == '.' || value == ':';
    }
}

bool IsValidProvisioningIdentifier(const std::string &value, size_t maximumLength)
{
    if (value.empty() || value.size() > maximumLength)
        return false;

    for (char character : value)
    {
        if (!IsAllowedIdentifierCharacter(character))
            return false;
    }

    return true;
}

bool ProvisioningRecord::IsValid() const
{
    if (version != CURRENT_VERSION)
        return false;

    switch (state)
    {
    case DeviceProvisioningState::Unprovisioned:
        return requestId.empty() && claimCode.empty() && userId.empty() && gardenId.empty();
    case DeviceProvisioningState::PendingClaim:
        return IsValidProvisioningIdentifier(requestId, 64) &&
               IsValidProvisioningIdentifier(claimCode, 32) &&
               userId.empty() && gardenId.empty();
    case DeviceProvisioningState::Provisioned:
        return IsValidProvisioningIdentifier(requestId, 64) && claimCode.empty() &&
               IsValidProvisioningIdentifier(userId) && IsValidProvisioningIdentifier(gardenId);
    }

    return false;
}

const char *ToString(DeviceProvisioningState state)
{
    switch (state)
    {
    case DeviceProvisioningState::Unprovisioned:
        return "unprovisioned";
    case DeviceProvisioningState::PendingClaim:
        return "pending_claim";
    case DeviceProvisioningState::Provisioned:
        return "provisioned";
    }

    return "unknown";
}

const char *ToString(ClaimDecision decision)
{
    switch (decision)
    {
    case ClaimDecision::Approved:
        return "approved";
    case ClaimDecision::Rejected:
        return "rejected";
    case ClaimDecision::Expired:
        return "expired";
    }

    return "unknown";
}

const char *ToString(ClaimProcessingResult result)
{
    switch (result)
    {
    case ClaimProcessingResult::Approved:
        return "approved";
    case ClaimProcessingResult::Rejected:
        return "rejected";
    case ClaimProcessingResult::Expired:
        return "expired";
    case ClaimProcessingResult::InvalidState:
        return "invalid_state";
    case ClaimProcessingResult::InvalidContract:
        return "invalid_contract";
    case ClaimProcessingResult::RequestMismatch:
        return "request_mismatch";
    case ClaimProcessingResult::AuthenticationFailed:
        return "authentication_failed";
    case ClaimProcessingResult::StorageError:
        return "storage_error";
    }

    return "unknown";
}

