#ifndef DEVICE_REGISTRY_ENTRY_H
#define DEVICE_REGISTRY_ENTRY_H

#include "Objects/FullTelemetry.h"
#include <optional>

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
    bool (*setState)(float, float, float, std::optional<bool>);
};

#endif