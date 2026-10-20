#ifndef DEVICE_REGISTRY_ENTRY_H
#define DEVICE_REGISTRY_ENTRY_H

#include "Objects/FullTelemetry.h"

struct DeviceRegistryEntry
{
    int id;
    void (*read)(FullTelemetry&);
};

#endif