#include "Device.h"
#include <stdint.h>

Device::Device(int id, uint8_t pin, const std::string &name, ConnectionType connType) : deviceId(id), devicePin(pin), deviceName(name), connType(connType)
{
};