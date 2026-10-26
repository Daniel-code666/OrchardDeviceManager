#include "SoilMoistureSensor.h"
#include "Constants/Constants.h"
#include "Objects/ConnectionType.h"
#include <Arduino.h>

SoilMoistureSensor::SoilMoistureSensor(int id, uint8_t pin, std::string instanceName, int airValue, int waterValue, int drySoilValue, int wetSoilValue)
    : Device(id, pin, instanceName, ConnectionType::Analog), airValue(airValue), waterValue(waterValue), drySoilValue(drySoilValue), wetSoilValue(wetSoilValue)
{
}

// incializa el sensor
void SoilMoistureSensor::Init()
{
    // Un canal analógico no permite detectar eléctricamente si la sonda está conectada.
    available = true;
}

// devuelve los valores captados por el sensor
SensorReading SoilMoistureSensor::GetSensorData()
{
    int rawValue = analogRead(devicePin);

    float moisturePercent = ConvertToPercentage(rawValue);

    return {
        deviceId,
        deviceName,
        measurementInfo,
        devicePin,
        connType,
        moisturePercent};
}

int SoilMoistureSensor::GetRawValue()
{
    return analogRead(devicePin);
}

// devuelve el valor del umbral en tierra seca
int SoilMoistureSensor::GetDrySoilValue()
{
    return drySoilValue;
}

// devuelve el valor del umbral en tierra mojada
int SoilMoistureSensor::GetWetSoilValue()
{
    return wetSoilValue;
}

// devuleve el porcentaje del umbral en tierra seca
float SoilMoistureSensor::GetDrySoilPercentage()
{
    return ConvertToPercentage(drySoilValue);
}

// devuelve el porcentaje del umbral en tierra mojada
float SoilMoistureSensor::GetWetSoilPercentage()
{
    return ConvertToPercentage(wetSoilValue);
}

float SoilMoistureSensor::ConvertToPercentage(int rawValue)
{
    float percentage = (float)(airValue - rawValue) * 100.0f / (airValue - waterValue);

    return constrain(percentage, 0.0f, 100.0f);
}
