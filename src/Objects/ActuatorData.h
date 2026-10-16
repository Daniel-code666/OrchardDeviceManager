#ifndef ACTUATOR_READING_H
#define ACTUATOR_READING_H

#include "Objects/ConnectionType.h"
#include "Objects/PumpControlMode.h"
#include <string>

struct ActuatorData {
    int actuatorId;
    std::string actuatorName;
    uint8_t actuatorPin;
    ConnectionType connType;
    PumpControlMode mode;
    float value;
};

#endif