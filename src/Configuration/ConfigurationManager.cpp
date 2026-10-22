#include "ConfigurationManager.h"

#include <Preferences.h>

namespace
{
    constexpr const char *KEY_COMMITTED = "committed";
    constexpr const char *KEY_VERSION = "cfg_ver";
    constexpr const char *KEY_DEVICE_ID = "device_id";
    constexpr const char *KEY_WIFI_SSID = "wifi_ssid";
    constexpr const char *KEY_WIFI_PASSWORD = "wifi_pass";
    constexpr const char *KEY_MQTT_HOST = "mqtt_host";
    constexpr const char *KEY_MQTT_PORT = "mqtt_port";
    constexpr const char *KEY_MQTT_CLIENT_ID = "mqtt_client";
    constexpr const char *KEY_MQTT_USER = "mqtt_user";
    constexpr const char *KEY_MQTT_PASSWORD = "mqtt_pass";
    constexpr const char *KEY_BACKUP_COMMITTED = "bak_commit";
    constexpr const char *KEY_BACKUP_VERSION = "bak_ver";
    constexpr const char *KEY_BACKUP_DEVICE_ID = "bak_device";
    constexpr const char *KEY_BACKUP_WIFI_SSID = "bak_ssid";
    constexpr const char *KEY_BACKUP_WIFI_PASSWORD = "bak_wifi_pass";
    constexpr const char *KEY_BACKUP_MQTT_HOST = "bak_mqtt_host";
    constexpr const char *KEY_BACKUP_MQTT_PORT = "bak_mqtt_port";
    constexpr const char *KEY_BACKUP_MQTT_CLIENT_ID = "bak_client";
    constexpr const char *KEY_BACKUP_MQTT_USER = "bak_user";
    constexpr const char *KEY_BACKUP_MQTT_PASSWORD = "bak_mqtt_pass";
    constexpr const char *KEY_PROVISIONING_COMMITTED = "prov_commit";
    constexpr const char *KEY_PROVISIONING_VERSION = "prov_ver";
    constexpr const char *KEY_PROVISIONING_STATE = "prov_state";
    constexpr const char *KEY_PROVISIONING_REQUEST = "prov_req";
    constexpr const char *KEY_PROVISIONING_CODE = "prov_code";
    constexpr const char *KEY_PROVISIONING_USER = "prov_user";
    constexpr const char *KEY_PROVISIONING_GARDEN = "prov_garden";
    constexpr const char *KEY_CREDENTIAL_COMMITTED = "cred_commit";
    constexpr const char *KEY_CREDENTIAL_VERSION = "cred_ver";
    constexpr const char *KEY_CREDENTIAL_KEY = "cred_key";
    constexpr const char *KEY_CREDENTIAL_APPLIED = "cred_applied";
    constexpr const char *KEY_CREDENTIAL_REQUEST = "cred_req";
    constexpr const char *KEY_RECOVERY_COMMITTED = "rec_commit";
    constexpr const char *KEY_RECOVERY_VERSION = "rec_ver";
    constexpr const char *KEY_RECOVERY_COMMAND_VERSION = "rec_cmd_ver";
    constexpr const char *KEY_RECOVERY_REQUEST = "rec_req";
    constexpr const char *KEY_RECOVERY_REVOKED = "rec_revoked";
    constexpr const char *KEY_RECOVERY_UNLINK = "rec_unlink";
    constexpr const char *KEY_OTA_COMMITTED = "ota_commit";
    constexpr const char *KEY_OTA_RECORD_VERSION = "ota_rec_ver";
    constexpr const char *KEY_OTA_STATE = "ota_state";
    constexpr const char *KEY_OTA_RELEASE = "ota_release";
    constexpr const char *KEY_OTA_TARGET = "ota_target";
    constexpr const char *KEY_OTA_FAILURE = "ota_failure";

    std::string ReadString(Preferences &preferences, const char *key)
    {
        const String value = preferences.getString(key, "");
        return std::string(value.c_str(), value.length());
    }

    bool WriteString(Preferences &preferences, const char *key, const std::string &value)
    {
        return preferences.putString(key, value.c_str()) > 0;
    }
}

ConfigurationLoadResult ConfigurationManager::Load(DeviceConfiguration &configuration) const
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true))
        return ConfigurationLoadResult::StorageError;

    if (!preferences.isKey(KEY_COMMITTED))
    {
        preferences.end();
        return ConfigurationLoadResult::NotFound;
    }

    if (!preferences.getBool(KEY_COMMITTED, false))
    {
        preferences.end();
        return ConfigurationLoadResult::Invalid;
    }

    DeviceConfiguration loaded;
    loaded.version = preferences.getUShort(KEY_VERSION, 0);
    loaded.deviceId = ReadString(preferences, KEY_DEVICE_ID);
    loaded.wifiSsid = ReadString(preferences, KEY_WIFI_SSID);
    loaded.wifiPassword = ReadString(preferences, KEY_WIFI_PASSWORD);
    loaded.mqttHost = ReadString(preferences, KEY_MQTT_HOST);
    loaded.mqttPort = preferences.getUShort(KEY_MQTT_PORT, 0);
    loaded.mqttClientId = ReadString(preferences, KEY_MQTT_CLIENT_ID);
    loaded.mqttUser = ReadString(preferences, KEY_MQTT_USER);
    loaded.mqttPassword = ReadString(preferences, KEY_MQTT_PASSWORD);
    preferences.end();

    if (!loaded.IsPersistable())
        return ConfigurationLoadResult::Invalid;

    configuration = std::move(loaded);
    return ConfigurationLoadResult::Loaded;
}

ConfigurationLoadResult ConfigurationManager::LoadRecoverable(
    DeviceConfiguration &configuration, bool &recoveredFromBackup) const
{
    recoveredFromBackup = false;
    const ConfigurationLoadResult primaryResult = Load(configuration);
    if (primaryResult == ConfigurationLoadResult::Loaded)
        return primaryResult;

    DeviceConfiguration backup;
    const ConfigurationLoadResult backupResult = LoadBackup(backup);
    if (backupResult != ConfigurationLoadResult::Loaded)
        return primaryResult;
    if (!Save(backup))
        return ConfigurationLoadResult::StorageError;

    configuration = std::move(backup);
    recoveredFromBackup = true;
    return ConfigurationLoadResult::Loaded;
}

bool ConfigurationManager::Save(const DeviceConfiguration &configuration) const
{
    if (!configuration.IsPersistable())
        return false;

    DeviceConfiguration previous;
    const bool hasPrevious = Load(previous) == ConfigurationLoadResult::Loaded;
    if (hasPrevious && !(previous == configuration) && !SaveBackup(previous))
        return false;

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false))
        return false;

    bool success = preferences.putBool(KEY_COMMITTED, false) > 0;
    success = WriteString(preferences, KEY_DEVICE_ID, configuration.deviceId) && success;
    success = WriteString(preferences, KEY_WIFI_SSID, configuration.wifiSsid) && success;
    success = WriteString(preferences, KEY_WIFI_PASSWORD, configuration.wifiPassword) && success;
    success = WriteString(preferences, KEY_MQTT_HOST, configuration.mqttHost) && success;
    success = preferences.putUShort(KEY_MQTT_PORT, configuration.mqttPort) > 0 && success;
    success = WriteString(preferences, KEY_MQTT_CLIENT_ID, configuration.mqttClientId) && success;
    success = WriteString(preferences, KEY_MQTT_USER, configuration.mqttUser) && success;
    success = WriteString(preferences, KEY_MQTT_PASSWORD, configuration.mqttPassword) && success;
    success = preferences.putUShort(KEY_VERSION, configuration.version) > 0 && success;

    if (success)
        success = preferences.putBool(KEY_COMMITTED, true) > 0;

    preferences.end();

    if (!success)
        return false;

    DeviceConfiguration persisted;
    const bool verified = Load(persisted) == ConfigurationLoadResult::Loaded &&
                          persisted == configuration;
    if (verified)
        SaveBackup(configuration);
    return verified;
}

ConfigurationLoadResult ConfigurationManager::LoadBackup(DeviceConfiguration &configuration) const
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true))
        return ConfigurationLoadResult::StorageError;
    if (!preferences.isKey(KEY_BACKUP_COMMITTED))
    {
        preferences.end();
        return ConfigurationLoadResult::NotFound;
    }
    if (!preferences.getBool(KEY_BACKUP_COMMITTED, false))
    {
        preferences.end();
        return ConfigurationLoadResult::Invalid;
    }

    DeviceConfiguration loaded;
    loaded.version = preferences.getUShort(KEY_BACKUP_VERSION, 0);
    loaded.deviceId = ReadString(preferences, KEY_BACKUP_DEVICE_ID);
    loaded.wifiSsid = ReadString(preferences, KEY_BACKUP_WIFI_SSID);
    loaded.wifiPassword = ReadString(preferences, KEY_BACKUP_WIFI_PASSWORD);
    loaded.mqttHost = ReadString(preferences, KEY_BACKUP_MQTT_HOST);
    loaded.mqttPort = preferences.getUShort(KEY_BACKUP_MQTT_PORT, 0);
    loaded.mqttClientId = ReadString(preferences, KEY_BACKUP_MQTT_CLIENT_ID);
    loaded.mqttUser = ReadString(preferences, KEY_BACKUP_MQTT_USER);
    loaded.mqttPassword = ReadString(preferences, KEY_BACKUP_MQTT_PASSWORD);
    preferences.end();

    if (!loaded.IsPersistable())
        return ConfigurationLoadResult::Invalid;
    configuration = std::move(loaded);
    return ConfigurationLoadResult::Loaded;
}

bool ConfigurationManager::SaveBackup(const DeviceConfiguration &configuration) const
{
    if (!configuration.IsPersistable())
        return false;

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false))
        return false;
    bool success = preferences.putBool(KEY_BACKUP_COMMITTED, false) > 0;
    success = WriteString(preferences, KEY_BACKUP_DEVICE_ID, configuration.deviceId) && success;
    success = WriteString(preferences, KEY_BACKUP_WIFI_SSID, configuration.wifiSsid) && success;
    success = WriteString(preferences, KEY_BACKUP_WIFI_PASSWORD, configuration.wifiPassword) && success;
    success = WriteString(preferences, KEY_BACKUP_MQTT_HOST, configuration.mqttHost) && success;
    success = preferences.putUShort(KEY_BACKUP_MQTT_PORT, configuration.mqttPort) > 0 && success;
    success = WriteString(preferences, KEY_BACKUP_MQTT_CLIENT_ID, configuration.mqttClientId) && success;
    success = WriteString(preferences, KEY_BACKUP_MQTT_USER, configuration.mqttUser) && success;
    success = WriteString(preferences, KEY_BACKUP_MQTT_PASSWORD, configuration.mqttPassword) && success;
    success = preferences.putUShort(KEY_BACKUP_VERSION, configuration.version) > 0 && success;
    if (success)
        success = preferences.putBool(KEY_BACKUP_COMMITTED, true) > 0;
    preferences.end();
    return success;
}

bool ConfigurationManager::UpdateWifi(const std::string &ssid, const std::string &password) const
{
    DeviceConfiguration configuration;
    if (Load(configuration) != ConfigurationLoadResult::Loaded)
        return false;

    configuration.wifiSsid = ssid;
    configuration.wifiPassword = password;
    return Save(configuration);
}

bool ConfigurationManager::UpdateMqtt(const std::string &host, uint16_t port,
                                      const std::string &clientId, const std::string &user,
                                      const std::string &password) const
{
    DeviceConfiguration configuration;
    if (Load(configuration) != ConfigurationLoadResult::Loaded)
        return false;

    configuration.mqttHost = host;
    configuration.mqttPort = port;
    configuration.mqttClientId = clientId;
    configuration.mqttUser = user;
    configuration.mqttPassword = password;
    return Save(configuration);
}

bool ConfigurationManager::ClearWifi() const
{
    DeviceConfiguration configuration;
    if (Load(configuration) != ConfigurationLoadResult::Loaded)
        return false;

    // La identidad y la configuracion MQTT se conservan intencionadamente.
    configuration.wifiSsid.clear();
    configuration.wifiPassword.clear();

    return Save(configuration);
}

ProvisioningLoadResult ConfigurationManager::LoadProvisioning(ProvisioningRecord &record) const
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true))
        return ProvisioningLoadResult::StorageError;

    if (!preferences.isKey(KEY_PROVISIONING_COMMITTED))
    {
        preferences.end();
        return ProvisioningLoadResult::NotFound;
    }

    if (!preferences.getBool(KEY_PROVISIONING_COMMITTED, false))
    {
        preferences.end();
        return ProvisioningLoadResult::Invalid;
    }

    ProvisioningRecord loaded;
    loaded.version = preferences.getUShort(KEY_PROVISIONING_VERSION, 0);
    loaded.state = static_cast<DeviceProvisioningState>(preferences.getUChar(KEY_PROVISIONING_STATE, 0xff));
    loaded.requestId = ReadString(preferences, KEY_PROVISIONING_REQUEST);
    loaded.claimCode = ReadString(preferences, KEY_PROVISIONING_CODE);
    loaded.userId = ReadString(preferences, KEY_PROVISIONING_USER);
    loaded.gardenId = ReadString(preferences, KEY_PROVISIONING_GARDEN);
    preferences.end();

    if (!loaded.IsValid())
        return ProvisioningLoadResult::Invalid;

    record = std::move(loaded);
    return ProvisioningLoadResult::Loaded;
}

bool ConfigurationManager::SaveProvisioning(const ProvisioningRecord &record) const
{
    if (!record.IsValid())
        return false;

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false))
        return false;

    bool success = preferences.putBool(KEY_PROVISIONING_COMMITTED, false) > 0;
    success = preferences.putUShort(KEY_PROVISIONING_VERSION, record.version) > 0 && success;
    success = preferences.putUChar(KEY_PROVISIONING_STATE, static_cast<uint8_t>(record.state)) > 0 && success;

    const auto writeOrRemove = [&preferences](const char *key, const std::string &value)
    {
        if (!value.empty())
            return preferences.putString(key, value.c_str()) == value.size();
        if (preferences.isKey(key))
            return preferences.remove(key);
        return true;
    };

    success = writeOrRemove(KEY_PROVISIONING_REQUEST, record.requestId) && success;
    success = writeOrRemove(KEY_PROVISIONING_CODE, record.claimCode) && success;
    success = writeOrRemove(KEY_PROVISIONING_USER, record.userId) && success;
    success = writeOrRemove(KEY_PROVISIONING_GARDEN, record.gardenId) && success;

    if (success)
        success = preferences.putBool(KEY_PROVISIONING_COMMITTED, true) > 0;
    preferences.end();

    if (!success)
        return false;

    ProvisioningRecord persisted;
    return LoadProvisioning(persisted) == ProvisioningLoadResult::Loaded &&
           persisted.version == record.version && persisted.state == record.state &&
           persisted.requestId == record.requestId && persisted.claimCode == record.claimCode &&
           persisted.userId == record.userId && persisted.gardenId == record.gardenId;
}

bool ConfigurationManager::ClearProvisioning() const
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false))
        return false;

    bool success = true;
    const char *keys[] = {
        KEY_PROVISIONING_COMMITTED, KEY_PROVISIONING_VERSION, KEY_PROVISIONING_STATE,
        KEY_PROVISIONING_REQUEST, KEY_PROVISIONING_CODE, KEY_PROVISIONING_USER,
        KEY_PROVISIONING_GARDEN};

    for (const char *key : keys)
    {
        if (preferences.isKey(key))
            success = preferences.remove(key) && success;
    }

    preferences.end();
    return success;
}

CredentialSecurityLoadResult ConfigurationManager::LoadCredentialSecurity(
    CredentialSecurityRecord &record) const
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true))
        return CredentialSecurityLoadResult::StorageError;
    if (!preferences.isKey(KEY_CREDENTIAL_COMMITTED))
    {
        preferences.end();
        return CredentialSecurityLoadResult::NotFound;
    }
    if (!preferences.getBool(KEY_CREDENTIAL_COMMITTED, false))
    {
        preferences.end();
        return CredentialSecurityLoadResult::Invalid;
    }

    CredentialSecurityRecord loaded;
    loaded.version = preferences.getUShort(KEY_CREDENTIAL_VERSION, 0);
    loaded.encryptionKeyHex = ReadString(preferences, KEY_CREDENTIAL_KEY);
    loaded.appliedCredentialVersion = preferences.getULong(KEY_CREDENTIAL_APPLIED, 0);
    loaded.pendingRequestId = ReadString(preferences, KEY_CREDENTIAL_REQUEST);
    preferences.end();

    if (!loaded.IsValid())
        return CredentialSecurityLoadResult::Invalid;
    record = std::move(loaded);
    return CredentialSecurityLoadResult::Loaded;
}

bool ConfigurationManager::SaveCredentialSecurity(const CredentialSecurityRecord &record) const
{
    if (!record.IsValid())
        return false;

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false))
        return false;

    bool success = preferences.putBool(KEY_CREDENTIAL_COMMITTED, false) > 0;
    success = preferences.putUShort(KEY_CREDENTIAL_VERSION, record.version) > 0 && success;
    success = preferences.putString(KEY_CREDENTIAL_KEY, record.encryptionKeyHex.c_str()) ==
                  record.encryptionKeyHex.size() && success;
    success = preferences.putULong(KEY_CREDENTIAL_APPLIED, record.appliedCredentialVersion) > 0 && success;
    if (record.pendingRequestId.empty())
    {
        if (preferences.isKey(KEY_CREDENTIAL_REQUEST))
            success = preferences.remove(KEY_CREDENTIAL_REQUEST) && success;
    }
    else
    {
        success = preferences.putString(KEY_CREDENTIAL_REQUEST, record.pendingRequestId.c_str()) ==
                      record.pendingRequestId.size() && success;
    }

    if (success)
        success = preferences.putBool(KEY_CREDENTIAL_COMMITTED, true) > 0;
    preferences.end();
    if (!success)
        return false;

    CredentialSecurityRecord persisted;
    return LoadCredentialSecurity(persisted) == CredentialSecurityLoadResult::Loaded &&
           persisted.version == record.version &&
           persisted.encryptionKeyHex == record.encryptionKeyHex &&
           persisted.appliedCredentialVersion == record.appliedCredentialVersion &&
           persisted.pendingRequestId == record.pendingRequestId;
}

bool ConfigurationManager::ClearCredentialSecurity() const
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false))
        return false;

    bool success = true;
    const char *keys[] = {
        KEY_CREDENTIAL_COMMITTED, KEY_CREDENTIAL_VERSION, KEY_CREDENTIAL_KEY,
        KEY_CREDENTIAL_APPLIED, KEY_CREDENTIAL_REQUEST};
    for (const char *key : keys)
    {
        if (preferences.isKey(key))
            success = preferences.remove(key) && success;
    }
    preferences.end();
    return success;
}

RecoveryLoadResult ConfigurationManager::LoadRecovery(RecoveryRecord &record) const
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true))
        return RecoveryLoadResult::StorageError;
    if (!preferences.isKey(KEY_RECOVERY_COMMITTED))
    {
        preferences.end();
        return RecoveryLoadResult::NotFound;
    }
    if (!preferences.getBool(KEY_RECOVERY_COMMITTED, false))
    {
        preferences.end();
        return RecoveryLoadResult::Invalid;
    }

    RecoveryRecord loaded;
    loaded.version = preferences.getUShort(KEY_RECOVERY_VERSION, 0);
    loaded.lastCommandVersion = preferences.getULong(KEY_RECOVERY_COMMAND_VERSION, 0);
    loaded.lastRequestId = ReadString(preferences, KEY_RECOVERY_REQUEST);
    loaded.mqttRevoked = preferences.getBool(KEY_RECOVERY_REVOKED, false);
    loaded.unlinkPending = preferences.getBool(KEY_RECOVERY_UNLINK, false);
    preferences.end();

    if (!loaded.IsValid())
        return RecoveryLoadResult::Invalid;
    record = std::move(loaded);
    return RecoveryLoadResult::Loaded;
}

bool ConfigurationManager::SaveRecovery(const RecoveryRecord &record) const
{
    if (!record.IsValid())
        return false;

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false))
        return false;

    bool success = preferences.putBool(KEY_RECOVERY_COMMITTED, false) > 0;
    success = preferences.putUShort(KEY_RECOVERY_VERSION, record.version) > 0 && success;
    success = preferences.putULong(KEY_RECOVERY_COMMAND_VERSION, record.lastCommandVersion) > 0 && success;
    if (record.lastRequestId.empty())
    {
        if (preferences.isKey(KEY_RECOVERY_REQUEST))
            success = preferences.remove(KEY_RECOVERY_REQUEST) && success;
    }
    else
    {
        success = preferences.putString(KEY_RECOVERY_REQUEST, record.lastRequestId.c_str()) ==
                      record.lastRequestId.size() && success;
    }
    success = preferences.putBool(KEY_RECOVERY_REVOKED, record.mqttRevoked) > 0 && success;
    success = preferences.putBool(KEY_RECOVERY_UNLINK, record.unlinkPending) > 0 && success;
    if (success)
        success = preferences.putBool(KEY_RECOVERY_COMMITTED, true) > 0;
    preferences.end();
    if (!success)
        return false;

    RecoveryRecord persisted;
    return LoadRecovery(persisted) == RecoveryLoadResult::Loaded &&
           persisted.version == record.version &&
           persisted.lastCommandVersion == record.lastCommandVersion &&
           persisted.lastRequestId == record.lastRequestId &&
           persisted.mqttRevoked == record.mqttRevoked &&
           persisted.unlinkPending == record.unlinkPending;
}

OtaLoadResult ConfigurationManager::LoadOta(OtaRecord &record) const
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true))
        return OtaLoadResult::StorageError;
    if (!preferences.isKey(KEY_OTA_COMMITTED))
    {
        preferences.end();
        return OtaLoadResult::NotFound;
    }
    if (!preferences.getBool(KEY_OTA_COMMITTED, false))
    {
        preferences.end();
        return OtaLoadResult::Invalid;
    }

    OtaRecord loaded;
    loaded.recordVersion = preferences.getUShort(KEY_OTA_RECORD_VERSION, 0);
    loaded.state = static_cast<OtaPersistentState>(preferences.getUChar(KEY_OTA_STATE, 0xff));
    loaded.releaseId = ReadString(preferences, KEY_OTA_RELEASE);
    loaded.targetVersion = ReadString(preferences, KEY_OTA_TARGET);
    loaded.failureCode = ReadString(preferences, KEY_OTA_FAILURE);
    preferences.end();
    if (!loaded.IsValid())
        return OtaLoadResult::Invalid;
    record = std::move(loaded);
    return OtaLoadResult::Loaded;
}

bool ConfigurationManager::SaveOta(const OtaRecord &record) const
{
    if (!record.IsValid())
        return false;

    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false))
        return false;
    bool success = preferences.putBool(KEY_OTA_COMMITTED, false) > 0;
    success = preferences.putUShort(KEY_OTA_RECORD_VERSION, record.recordVersion) > 0 && success;
    success = preferences.putUChar(KEY_OTA_STATE, static_cast<uint8_t>(record.state)) > 0 && success;

    const auto writeOrRemove = [&preferences](const char *key, const std::string &value)
    {
        if (!value.empty())
            return preferences.putString(key, value.c_str()) == value.size();
        if (preferences.isKey(key))
            return preferences.remove(key);
        return true;
    };
    success = writeOrRemove(KEY_OTA_RELEASE, record.releaseId) && success;
    success = writeOrRemove(KEY_OTA_TARGET, record.targetVersion) && success;
    success = writeOrRemove(KEY_OTA_FAILURE, record.failureCode) && success;
    if (success)
        success = preferences.putBool(KEY_OTA_COMMITTED, true) > 0;
    preferences.end();
    if (!success)
        return false;

    OtaRecord persisted;
    return LoadOta(persisted) == OtaLoadResult::Loaded &&
           persisted.recordVersion == record.recordVersion && persisted.state == record.state &&
           persisted.releaseId == record.releaseId &&
           persisted.targetVersion == record.targetVersion &&
           persisted.failureCode == record.failureCode;
}
