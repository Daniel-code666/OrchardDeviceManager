#pragma once

#include "DeviceProvisioningManager.h"
#include "../CredentialProvisioning/MqttCredentialProvisioningManager.h"
#include "../Recovery/DeviceRecoveryManager.h"
#include "../Ota/FirmwareUpdateManager.h"
#include "../Provisioning/ProvisioningManager.h"
#include "../Sensors/Sensors.h"

#include <string>

class SerialProvisioningMock
{
public:
    SerialProvisioningMock(ProvisioningManager &wifiProvisioningManager,
                           DeviceProvisioningManager &manager,
                           MqttCredentialProvisioningManager &credentialManager,
                           DeviceRecoveryManager &recoveryManager,
                           FirmwareUpdateManager &firmwareUpdateManager,
                           Sensors &sensors);

    void AnnouncePendingRequest() const;
    void AnnouncePendingCredentialRequest() const;
    void Loop(unsigned long now);
    void SetDeviceInfo(const std::string &deviceId,
                       const std::string &mqttClientId,
                       const std::string &firmwareVersion);

private:
    static constexpr size_t MAX_RESPONSE_LENGTH = 6144;

    ProvisioningManager &wifiProvisioningManager;
    DeviceProvisioningManager &manager;
    MqttCredentialProvisioningManager &credentialManager;
    DeviceRecoveryManager &recoveryManager;
    FirmwareUpdateManager &firmwareUpdateManager;
    Sensors &sensors;
    std::string inputBuffer;
    std::string deviceId;
    std::string mqttClientId;
    std::string firmwareVersion;

    void ProcessLine(unsigned long now);
};

