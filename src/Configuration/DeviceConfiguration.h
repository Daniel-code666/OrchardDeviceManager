#pragma once

#include <cstdint>
#include <string>

struct DeviceConfiguration
{
    static constexpr uint16_t CURRENT_VERSION = 1;

    std::string deviceId;
    std::string wifiSsid;
    std::string wifiPassword;
    std::string mqttHost;
    uint16_t mqttPort = 0;
    std::string mqttClientId;
    std::string mqttUser;
    std::string mqttPassword;
    uint16_t version = CURRENT_VERSION;

    bool IsValid() const;
    bool IsPersistable() const;
    bool HasWifiConfiguration() const;

    bool operator==(const DeviceConfiguration &other) const;
};
