#pragma once

#include "DeviceConfiguration.h"
#include "../DeviceProvisioning/ProvisioningTypes.h"
#include "../CredentialProvisioning/CredentialProvisioningTypes.h"
#include "../Recovery/RecoveryTypes.h"
#include "../Ota/OtaTypes.h"

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

enum class RecoveryLoadResult
{
    Loaded,
    NotFound,
    Invalid,
    StorageError
};

enum class OtaLoadResult
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
    ConfigurationLoadResult LoadRecoverable(DeviceConfiguration &configuration,
                                            bool &recoveredFromBackup) const;
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
    bool ClearCredentialSecurity() const;

    RecoveryLoadResult LoadRecovery(RecoveryRecord &record) const;
    bool SaveRecovery(const RecoveryRecord &record) const;

    OtaLoadResult LoadOta(OtaRecord &record) const;
    bool SaveOta(const OtaRecord &record) const;

private:
    static constexpr const char *NAMESPACE = "huerta_cfg";

    ConfigurationLoadResult LoadBackup(DeviceConfiguration &configuration) const;
    bool SaveBackup(const DeviceConfiguration &configuration) const;
};
