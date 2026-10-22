#pragma once

#include "DeviceProvisioningManager.h"
#include "../CredentialProvisioning/MqttCredentialProvisioningManager.h"
#include "../Recovery/DeviceRecoveryManager.h"
#include "../Ota/FirmwareUpdateManager.h"
#include "../Sensors/Sensors.h"

#include <string>

class SerialProvisioningMock
{
public:
    SerialProvisioningMock(DeviceProvisioningManager &manager,
                           MqttCredentialProvisioningManager &credentialManager,
                           DeviceRecoveryManager &recoveryManager,
                           FirmwareUpdateManager &firmwareUpdateManager,
                           Sensors &sensors);

    void AnnouncePendingRequest() const;
    void AnnouncePendingCredentialRequest() const;
    void Loop(unsigned long now);

private:
    static constexpr size_t MAX_RESPONSE_LENGTH = 6144;

    DeviceProvisioningManager &manager;
    MqttCredentialProvisioningManager &credentialManager;
    DeviceRecoveryManager &recoveryManager;
    FirmwareUpdateManager &firmwareUpdateManager;
    Sensors &sensors;
    std::string inputBuffer;

    void ProcessLine(unsigned long now);
};

