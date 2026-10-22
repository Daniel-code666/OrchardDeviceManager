#pragma once

#include "RecoveryTypes.h"
#include "../Configuration/ConfigurationManager.h"
#include "../DeviceProvisioning/DeviceProvisioningManager.h"

#include <string>

class DeviceRecoveryManager
{
public:
    DeviceRecoveryManager(ConfigurationManager &configurationManager,
                          DeviceProvisioningManager &provisioningManager);

    bool Begin(const std::string &deviceId);
    RecoveryCommandResult ProcessCommand(const RecoveryCommand &command,
                                         const std::string &authenticationKeyHex);
    bool IsMqttRevoked() const;
    bool MarkCredentialsRestored();

private:
    ConfigurationManager &configurationManager;
    DeviceProvisioningManager &provisioningManager;
    RecoveryRecord record;
    std::string deviceId;
    bool initialized;

    bool SaveRecord(const RecoveryRecord &candidate);
    bool CompletePendingUnlink();
};

