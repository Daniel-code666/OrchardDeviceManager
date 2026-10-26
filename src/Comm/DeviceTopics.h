#pragma once

#include <string>

struct DeviceTopics
{
    std::string telemetry;
    std::string actions;
    std::string actionsResponse;
    std::string inventory;
    std::string control;
    std::string controlStatus;

    static bool TryCreate(const std::string &deviceId, DeviceTopics &topics);
    static bool IsValidDeviceId(const std::string &deviceId);
};
