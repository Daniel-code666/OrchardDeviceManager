#ifndef BASIC_SENSOR_ACTUATOR_INFO_H
#define BASIC_SENSOR_ACTUATOR_INFO_H

#include <stdint.h>
#include "Objects/SensorReading.h"
#include "Objects/MeasurementInfo.h"
#include "Objects/ConnectionType.h"

class Device
{
    public:
        Device(int id, uint8_t pin, const std::string &name, ConnectionType connType);

        virtual ~Device() = default;

        virtual void Init() = 0;
        int GetDeviceId();
        bool IsAvailable() const;
    protected:
        const int deviceId;
        const uint8_t devicePin;
        const std::string deviceName;
        const ConnectionType connType;
        bool available = false;
};

#endif
