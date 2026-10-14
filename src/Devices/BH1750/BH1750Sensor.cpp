#include "BH1750Sensor.h"
#include <Wire.h>
#include <BH1750.h>
#include "Constants/Constants.h"

BH1750Sensor::BH1750Sensor(int id) : Device(id, NO_PIN, "BH1750", ConnectionType::I2C)
{
}

void BH1750Sensor::Init()
{
    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, i2cAddress))
        Serial.print("Conexión fallida \n");
    else
        Serial.print("Conexión exitosa, iniciado sensor");
}

SensorReading BH1750Sensor::GetSensorData()
{
    float lux = lightMeter.readLightLevel();

    return {
        deviceId,
        deviceName,
        measurementInfo,
        devicePin,
        connType,
        lux
    };
}