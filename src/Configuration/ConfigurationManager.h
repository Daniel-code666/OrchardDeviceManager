#pragma once

#include "DeviceConfiguration.h"
#include "../DeviceProvisioning/ProvisioningTypes.h"
#include "../CredentialProvisioning/CredentialProvisioningTypes.h"

enum class ConfigurationLoadResult
{
    Loaded,
    NotFound,
    Invalid,
    StorageError
};

enum class ProvisioningLoadResult
{
    Loaded,
    NotFound,
    Invalid,
    StorageError
};

enum class CredentialSecurityLoadResult
{
    Loaded,
    NotFound,
    Invalid,
    StorageError
};

class ConfigurationManager
{
public:
    ConfigurationLoadResult Load(DeviceConfiguration &configuration) const;
    bool Save(const DeviceConfiguration &configuration) const;

    bool UpdateWifi(const std::string &ssid, const std::string &password) const;
    bool UpdateMqtt(const std::string &host, uint16_t port,
                    const std::string &clientId, const std::string &user,
                    const std::string &password) const;
    bool ClearWifi() const;

    ProvisioningLoadResult LoadProvisioning(ProvisioningRecord &record) const;
    bool SaveProvisioning(const ProvisioningRecord &record) const;
    bool ClearProvisioning() const;

    CredentialSecurityLoadResult LoadCredentialSecurity(CredentialSecurityRecord &record) const;
    bool SaveCredentialSecurity(const CredentialSecurityRecord &record) const;

private:
    static constexpr const char *NAMESPACE = "huerta_cfg";
};
