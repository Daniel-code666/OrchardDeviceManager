#pragma once

#include "CredentialProvisioningTypes.h"

#include <string>

class CredentialProvisioningJson
{
public:
    static std::string Serialize(const MqttCredentialRequest &request);
    static bool DeserializeEnvelope(const std::string &json, EncryptedMqttCredentials &envelope);
};

