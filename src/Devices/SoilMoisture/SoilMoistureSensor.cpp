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
    // actualmente no es necesario inicializar nada a parte de establecer parámetros para el ambiente seco o húmedo que pueden ser costantes en la clase
}

// devuelve los valores captados por el sensor
SensorReading SoilMoistureSensor::GetSensorData()
{
    int rawValue = analogRead(devicePin);

    float moisturePercent = (float) (airValue - rawValue) * 100.0f / (airValue - waterValue); 

    moisturePercent = constrain(moisturePercent, 0, 100);

    return {
        deviceId,
        deviceName,
        measurementInfo,
        devicePin,
        connType,
        moisturePercent
    };
}