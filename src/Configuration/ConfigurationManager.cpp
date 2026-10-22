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

bool ConfigurationManager::Save(const DeviceConfiguration &configuration) const
{
    if (!configuration.IsPersistable())
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
    return Load(persisted) == ConfigurationLoadResult::Loaded && persisted == configuration;
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
