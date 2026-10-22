#include "DeviceIdentity.h"

#include "../Comm/DeviceTopics.h"

#include <cstdio>
#include <esp_err.h>
#include <esp_mac.h>

bool DeviceIdentity::TryCreate(DeviceIdentity &identity)
{
    std::array<uint8_t, 6> mac{};
    if (esp_read_mac(mac.data(), ESP_MAC_WIFI_STA) != ESP_OK)
        return false;

    return FromMac(mac, identity);
}

bool DeviceIdentity::FromMac(const std::array<uint8_t, 6> &mac, DeviceIdentity &identity)
{
    bool allZero = true;
    bool allOnes = true;
    for (const uint8_t value : mac)
    {
        allZero = allZero && value == 0;
        allOnes = allOnes && value == 0xff;
    }
    if (allZero || allOnes)
        return false;

    char hardwareId[13]{};
    std::snprintf(hardwareId, sizeof(hardwareId), "%02x%02x%02x%02x%02x%02x",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    DeviceIdentity generated;
    generated.hardwareId = hardwareId;
    generated.deviceId = "huerta-c3-" + generated.hardwareId;
    generated.mqttClientId = "mqtt-" + generated.deviceId;
    if (!DeviceTopics::IsValidDeviceId(generated.deviceId) ||
        generated.mqttClientId.size() > 128)
        return false;

    identity = std::move(generated);
    return true;
}
