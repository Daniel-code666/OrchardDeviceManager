#pragma once

#include "ManagedProvisioningTypes.h"
#include "../Configuration/DeviceConfiguration.h"
#include "../DeviceProvisioning/ProvisioningTypes.h"
#include "../Identity/DeviceIdentity.h"

#include <string>

class ManagedProvisioningJson
{
public:
    static bool TryGetMessageType(const std::string &json, std::string &type);
    static bool DeserializeBootstrap(const std::string &json, ManagedBootstrapRequest &request);
    static std::string SerializeIdentifyResult(const std::string &requestId,
                                               const DeviceIdentity &identity,
                                               const std::string &firmwareVersion,
                                               DeviceProvisioningState provisioningState,
                                               const DeviceConfiguration &configuration);
    static std::string SerializeStatus(const std::string &requestId,
                                       const char *stage,
                                       const char *status,
                                       const char *errorCode = "");
    static std::string SerializeClaimRegistration(const ClaimRequest &claim,
                                                  const DeviceIdentity &identity,
                                                  const std::string &firmwareVersion);
};

