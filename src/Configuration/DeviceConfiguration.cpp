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
    return IsPersistable() && HasWifiConfiguration();
}

bool DeviceConfiguration::IsPersistable() const
{
    return version == CURRENT_VERSION &&
           DeviceTopics::IsValidDeviceId(deviceId) &&
           wifiSsid.size() <= 32 &&
           wifiPassword.size() <= 64 &&
           HasValidLength(mqttHost, 253) &&
           mqttPort != 0 &&
           HasValidLength(mqttClientId, 128) &&
           mqttUser.size() <= 256 &&
           mqttPassword.size() <= 512;
}

bool DeviceConfiguration::HasWifiConfiguration() const
{
    return HasValidLength(wifiSsid, 32) && wifiPassword.size() <= 64;
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
