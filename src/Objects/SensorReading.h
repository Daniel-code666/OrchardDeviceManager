#ifndef SENSOR_READING_H
#define SENSOR_READING_H

#include <string>
#include "MeasurementInfo.h"
#include "ConnectionType.h"

struct SensorReading
{
    std::string sensorName;
    MeasurementInfo measurementInfo;
    uint8_t sensorPin;
    ConnectionType connType;
    float value;
};

#endif