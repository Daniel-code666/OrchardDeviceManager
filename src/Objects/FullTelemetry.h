#ifndef FULL_TELEMETRY_H
#define FULL_TELEMETRY_H

#include <vector>
#include "Objects/SensorReading.h"
#include "Objects/ActuatorData.h"

class FullTelemetry
{
    public:
        std::vector<SensorReading> sensorReadings;
        std::vector<ActuatorData> actuatorsData;
};

#endif
