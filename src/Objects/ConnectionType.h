#ifndef CONNECTION_TYPE_H
#define CONNECTION_TYPE_H

#include <stdint.h>

enum class ConnectionType : uint8_t
{
    OneWire,
    I2C,
    Analog,
    Digital
};

#endif