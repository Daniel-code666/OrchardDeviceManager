#ifndef MEASUREMENT_TYPES_H
#define MEASUREMENT_TYPES_H

#include <stdint.h>

enum class MeasurementType : uint8_t
{
    SoilTemperature,
    AirTemperature,
    AirHumidity,
    Illuminance,
    SoilMoisture
};

#endif

