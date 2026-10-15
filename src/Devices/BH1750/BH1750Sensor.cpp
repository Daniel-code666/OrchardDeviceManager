#include "BH1750Sensor.h"
#include <Wire.h>
#include <BH1750.h>
#include "Constants/Constants.h"

BH1750Sensor::BH1750Sensor(int id) : Device(id, NO_PIN, "BH1750", ConnectionType::I2C)
{
}

void BH1750Sensor::Init()
{
    bool isInitialized = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, i2cAddress, &Wire);

    Serial.println(isInitialized ? "Inicializado BH1750" : "No inicializado BH1750");
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