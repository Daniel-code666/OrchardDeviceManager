#include "DeviceTopics.h"

bool DeviceTopics::TryCreate(const std::string &deviceId, DeviceTopics &topics)
{
    if (!IsValidDeviceId(deviceId))
        return false;

    const std::string prefix = "gardens/" + deviceId + "/";
    topics.telemetry = prefix + "telemetry";
    topics.actions = prefix + "actions";
    topics.actionsResponse = prefix + "actions-response";
    return true;
}

bool DeviceTopics::IsValidDeviceId(const std::string &deviceId)
{
    if (deviceId.empty() || deviceId.size() > 64)
        return false;

    for (const char value : deviceId)
    {
        const bool isAlphaNumeric = (value >= 'a' && value <= 'z') ||
                                    (value >= 'A' && value <= 'Z') ||
                                    (value >= '0' && value <= '9');
        if (!isAlphaNumeric && value != '-' && value != '_')
            return false;
    }

    return true;
}
