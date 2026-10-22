#include "DeviceRecoveryManager.h"

#include "RecoveryAuthentication.h"
#include "../DeviceProvisioning/ProvisioningTypes.h"

DeviceRecoveryManager::DeviceRecoveryManager(ConfigurationManager &configuration,
                                             DeviceProvisioningManager &provisioning)
    : configurationManager(configuration), provisioningManager(provisioning), initialized(false)
{
}

bool DeviceRecoveryManager::Begin(const std::string &stableDeviceId)
{
    if (!IsValidProvisioningIdentifier(stableDeviceId))
        return false;

    deviceId = stableDeviceId;
    RecoveryRecord loaded;
    const RecoveryLoadResult result = configurationManager.LoadRecovery(loaded);
    if (result == RecoveryLoadResult::Loaded)
    {
        record = std::move(loaded);
    }
    else if (result == RecoveryLoadResult::NotFound)
    {
        if (!SaveRecord(RecoveryRecord{}))
            return false;
    }
    else
    {
        // Ante un registro dudoso se aplica el estado seguro: MQTT bloqueado.
        RecoveryRecord safeRecord;
        safeRecord.mqttRevoked = true;
        if (!SaveRecord(safeRecord))
            return false;
    }

    initialized = true;
    return !record.unlinkPending || CompletePendingUnlink();
}

RecoveryCommandResult DeviceRecoveryManager::ProcessCommand(
    const RecoveryCommand &command, const std::string &authenticationKeyHex)
{
    if (!initialized || !provisioningManager.IsProvisioned())
        return RecoveryCommandResult::InvalidState;
    if (command.contractVersion != RecoveryCommand::CONTRACT_VERSION ||
        command.deviceId != deviceId || !IsValidProvisioningIdentifier(command.deviceId) ||
        !IsValidProvisioningIdentifier(command.requestId, 64) ||
        command.commandVersion == 0)
        return RecoveryCommandResult::InvalidContract;
    if (command.commandVersion <= record.lastCommandVersion ||
        command.requestId == record.lastRequestId)
        return RecoveryCommandResult::ReplayRejected;
    if (!RecoveryAuthentication::Verify(command, authenticationKeyHex))
        return RecoveryCommandResult::AuthenticationFailed;

    RecoveryRecord candidate = record;
    candidate.lastCommandVersion = command.commandVersion;
    candidate.lastRequestId = command.requestId;
    candidate.mqttRevoked = true;
    candidate.unlinkPending = command.action == RecoveryAction::Unlink;
    if (!SaveRecord(candidate))
        return RecoveryCommandResult::StorageError;

    if (command.action == RecoveryAction::Unlink && !CompletePendingUnlink())
        return RecoveryCommandResult::StorageError;
    return RecoveryCommandResult::Applied;
}

bool DeviceRecoveryManager::IsMqttRevoked() const
{
    return !initialized || record.mqttRevoked;
}

bool DeviceRecoveryManager::MarkCredentialsRestored()
{
    if (!initialized || record.unlinkPending)
        return false;
    if (!record.mqttRevoked)
        return true;

    RecoveryRecord candidate = record;
    candidate.mqttRevoked = false;
    return SaveRecord(candidate);
}

bool DeviceRecoveryManager::SaveRecord(const RecoveryRecord &candidate)
{
    if (!configurationManager.SaveRecovery(candidate))
        return false;
    record = candidate;
    return true;
}

bool DeviceRecoveryManager::CompletePendingUnlink()
{
    if (!record.unlinkPending)
        return true;

    if (!provisioningManager.AuthorizeUnlink())
        return false;
    if (!configurationManager.ClearCredentialSecurity())
        return false;

    RecoveryRecord completed = record;
    completed.unlinkPending = false;
    completed.mqttRevoked = true;
    return SaveRecord(completed);
}

