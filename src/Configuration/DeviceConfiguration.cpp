#include "DeviceConfiguration.h"
#include "../Comm/DeviceTopics.h"

namespace
{
    bool HasValidLength(const std::string &value, size_t maximum)
    {
        return !value.empty() && value.size() <= maximum;
    }
}

bool DeviceConfiguration::IsValid() const
{
    return IsPersistable() && HasWifiConfiguration() && HasMqttConfiguration();
}

bool DeviceConfiguration::IsPersistable() const
{
    const bool wifiFieldsAreConsistent = !wifiSsid.empty() || wifiPassword.empty();
    const bool mqttFieldsAreEmpty = mqttHost.empty() && mqttPort == 0 &&
                                    mqttUser.empty() && mqttPassword.empty();
    const bool mqttFieldsAreComplete = HasMqttConfiguration();

    return version == CURRENT_VERSION &&
           DeviceTopics::IsValidDeviceId(deviceId) &&
           wifiSsid.size() <= 32 &&
           wifiPassword.size() <= 64 &&
           HasValidLength(mqttClientId, 128) &&
           wifiFieldsAreConsistent &&
           (mqttFieldsAreEmpty || mqttFieldsAreComplete);
}

bool DeviceConfiguration::HasWifiConfiguration() const
{
    return HasValidLength(wifiSsid, 32) && wifiPassword.size() <= 64;
}

bool DeviceConfiguration::HasMqttConfiguration() const
{
    return HasValidLength(mqttHost, 253) &&
           mqttPort != 0 &&
           HasValidLength(mqttClientId, 128) &&
           HasValidLength(mqttUser, 256) &&
           HasValidLength(mqttPassword, 512);
}

bool DeviceConfiguration::operator==(const DeviceConfiguration &other) const
{
    return deviceId == other.deviceId &&
           wifiSsid == other.wifiSsid &&
           wifiPassword == other.wifiPassword &&
           mqttHost == other.mqttHost &&
           mqttPort == other.mqttPort &&
           mqttClientId == other.mqttClientId &&
           mqttUser == other.mqttUser &&
           mqttPassword == other.mqttPassword &&
           version == other.version;
}
