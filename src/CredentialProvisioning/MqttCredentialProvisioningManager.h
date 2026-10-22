#pragma once

#include "CredentialProvisioningTypes.h"
#include "../Configuration/ConfigurationManager.h"

#include <string>

class MqttCredentialProvisioningManager
{
public:
    explicit MqttCredentialProvisioningManager(ConfigurationManager &configurationManager);

    bool Begin(const std::string &deviceId, const DeviceConfiguration &activeConfiguration,
               bool deviceIsProvisioned);
    void UpdateBaseConfiguration(const DeviceConfiguration &activeConfiguration);
    bool StartRequest();
    CredentialEnvelopeResult ProcessEnvelope(const EncryptedMqttCredentials &envelope);

    bool TryGetPendingRequest(MqttCredentialRequest &request) const;
    bool TryGetLocalBootstrapKey(std::string &keyHex) const;
    bool TryTakeCandidate(DeviceConfiguration &configuration, uint32_t &credentialVersion);
    bool ConfirmApplied(uint32_t credentialVersion);
    void RejectCandidate();
    uint32_t GetAppliedCredentialVersion() const;

private:
    ConfigurationManager &configurationManager;
    CredentialSecurityRecord securityRecord;
    DeviceConfiguration baseConfiguration;
    DeviceConfiguration candidateConfiguration;
    std::string deviceId;
    uint32_t candidateVersion;
    bool initialized;
    bool candidatePending;

    bool SaveSecurityRecord(const CredentialSecurityRecord &candidate);
    static std::string GenerateRandomHex(size_t byteCount);
    static std::string BuildAdditionalAuthenticatedData(const EncryptedMqttCredentials &envelope);
    static bool DecodeHex(const std::string &hex, std::string &bytes);
    static bool Decrypt(const EncryptedMqttCredentials &envelope, const std::string &keyHex,
                        std::string &plaintext);
    bool ParseCredentials(const std::string &plaintext, DeviceConfiguration &configuration,
                          uint32_t &credentialVersion) const;
};

