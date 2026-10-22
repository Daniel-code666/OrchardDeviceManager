#pragma once

#include <array>
#include <cstdint>
#include <string>

struct DeviceIdentity
{
    std::string deviceId;
    std::string mqttClientId;
    std::string hardwareId;

    static bool TryCreate(DeviceIdentity &identity);
    static bool FromMac(const std::array<uint8_t, 6> &mac, DeviceIdentity &identity);
};
