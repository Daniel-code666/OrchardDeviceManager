#pragma once

#include <string>

struct DeviceTopics
{
    std::string telemetry;
    std::string actions;
    std::string actionsResponse;

    static bool TryCreate(const std::string &deviceId, DeviceTopics &topics);
    static bool IsValidDeviceId(const std::string &deviceId);
};
