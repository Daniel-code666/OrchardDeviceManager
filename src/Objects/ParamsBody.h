#ifndef PARAMS_BODY_H
#define PARAMS_BODY_H

#include "DeviceType.h"

struct ParamsBody
{
    int deviceId;
    DeviceType deviceType;
    bool state;
    float value;
};

#endif