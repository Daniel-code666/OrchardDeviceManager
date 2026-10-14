#ifndef MEASUREMENT_INFO_H
#define MEASUREMENT_INFO_H

#include "MeasurementTypes.h"

struct MeasurementInfo
{
    MeasurementType type;
    const char* name;
    const char* unit;
};

#endif