#ifndef DEVICE_REGISTRY_ENTRY_H
#define DEVICE_REGISTRY_ENTRY_H

#include "Objects/FullTelemetry.h"

struct SensorRegistryEntry
{
    int id;
    void (*read)(FullTelemetry&);
};

struct ActuatorRegistryEntry
{
    int id;
    void (*read)(FullTelemetry&);
    bool (*setMode)(int);
};

#endif