#include "RecoveryTypes.h"

#include "../DeviceProvisioning/ProvisioningTypes.h"

bool RecoveryRecord::IsValid() const
{
    return version == CURRENT_VERSION &&
           (lastRequestId.empty() || IsValidProvisioningIdentifier(lastRequestId, 64)) &&
           (!unlinkPending || mqttRevoked);
}

const char *ToString(RecoveryAction action)
{
    switch (action)
    {
    case RecoveryAction::RevokeMqtt:
        return "revoke_mqtt";
    case RecoveryAction::Unlink:
        return "unlink";
    }
    return "unknown";
}

const char *ToString(RecoveryCommandResult result)
{
    switch (result)
    {
    case RecoveryCommandResult::Applied:
        return "applied";
    case RecoveryCommandResult::InvalidState:
        return "invalid_state";
    case RecoveryCommandResult::InvalidContract:
        return "invalid_contract";
    case RecoveryCommandResult::AuthenticationFailed:
        return "authentication_failed";
    case RecoveryCommandResult::ReplayRejected:
        return "replay_rejected";
    case RecoveryCommandResult::StorageError:
        return "storage_error";
    }
    return "unknown";
}

