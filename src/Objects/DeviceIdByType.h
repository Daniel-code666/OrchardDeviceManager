#ifndef DEVICE_ID_BY_TYPE_H
#define DEVICE_ID_BY_TYPE_H

#include <vector>

struct DeviceIdByType
{
    std::vector<int> sensorIds;
    std::vector<int> actuatorIds;
};

#endif