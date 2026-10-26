#pragma once

#include "../Comm/MqttCommunication.h"
#include "../CredentialProvisioning/MqttCredentialProvisioningManager.h"
#include "../DeviceProvisioning/DeviceProvisioningManager.h"
#include "../Ota/FirmwareUpdateManager.h"
#include "../Recovery/DeviceRecoveryManager.h"
#include "../Sensors/Sensors.h"

#include <string>

class RemoteControlManager
{
public:
    RemoteControlManager(MqttCommunication &communication,
                         FirmwareUpdateManager &firmwareManager,
                         MqttCredentialProvisioningManager &credentialManager,
                         DeviceProvisioningManager &provisioningManager,
                         DeviceRecoveryManager &recoveryManager,
                         Sensors &sensors);

    bool Begin(const std::string &deviceId);
    void ProcessMessage(const std::string &message, unsigned long now);
    void Loop(unsigned long now);

private:
    MqttCommunication &communication;
    FirmwareUpdateManager &firmwareManager;
    MqttCredentialProvisioningManager &credentialManager;
    DeviceProvisioningManager &provisioningManager;
    DeviceRecoveryManager &recoveryManager;
    Sensors &sensors;
    std::string deviceId;
    std::string pendingStatus;
    OtaRuntimeState lastRuntimeState;
    uint8_t lastProgress;
    unsigned long lastStatusAt;
    bool initialized;
    bool clockConfigured;

    void ProcessOtaRequest(const std::string &message, unsigned long now);
    void ProcessRecoveryRequest(const std::string &message);
    void QueueOtaStatus(const std::string &requestId, const std::string &releaseId,
                        const char *status, uint8_t progress,
                        const std::string &targetVersion, const char *failureCode);
    void QueuePersistentOtaStatus();
    static bool ParseUtc(const std::string &value, time_t &timestamp);
};

