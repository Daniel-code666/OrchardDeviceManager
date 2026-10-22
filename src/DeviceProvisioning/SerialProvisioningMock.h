#pragma once

#include "DeviceProvisioningManager.h"
#include "../CredentialProvisioning/MqttCredentialProvisioningManager.h"

#include <string>

class SerialProvisioningMock
{
public:
    SerialProvisioningMock(DeviceProvisioningManager &manager,
                           MqttCredentialProvisioningManager &credentialManager);

    void AnnouncePendingRequest() const;
    void AnnouncePendingCredentialRequest() const;
    void Loop(unsigned long now);

private:
    static constexpr size_t MAX_RESPONSE_LENGTH = 6144;

    DeviceProvisioningManager &manager;
    MqttCredentialProvisioningManager &credentialManager;
    std::string inputBuffer;

    void ProcessLine(unsigned long now);
};

