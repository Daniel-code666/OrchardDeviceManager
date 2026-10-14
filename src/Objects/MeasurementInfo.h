#ifndef MEASUREMENT_INFO_H
#define MEASUREMENT_INFO_H

#include "MeasurementTypes.h"
#include "MeasurementUnits.h"

struct MeasurementInfo
{
    MeasurementType type;
    const char* name;
    MeasurementUnits unit;
};

#endif