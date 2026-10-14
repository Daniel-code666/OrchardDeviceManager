#include "BH1750Sensor.h"

BH1750Sensor::BH1750Sensor(int id, u_int8_t pin) : Device(id, pin, "BH1750", ConnectionType::I2C)
{
}

void BH1750Sensor::Init()
{

}

SensorReading BH1750Sensor::GetSensorData()
{
    return {
        deviceName,
        measurementInfo,
        devicePin,
        connType,
        0
    };
}