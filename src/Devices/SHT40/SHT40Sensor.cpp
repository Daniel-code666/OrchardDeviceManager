#include "SHT40Sensor.h"
#include "Constants/Constants.h"
#include <Wire.h>

SHT40Sensor::SHT40Sensor(int id) : Device(id, NO_PIN, "SHT40", ConnectionType::I2C)
{
}

// inicializa el sensor
void SHT40Sensor::Init()
{
    bool isInitialized = sht4.begin();

    if (!isInitialized)
    {
        Serial.print("SHT40 no inicializado");
    }
    else
    {
        sht4.setPrecision(SHT4X_HIGH_PRECISION);
        Serial.print("SH40 Inicializado");
    }
}

// obtiene los datos del sensor, la opción indica si devuelve los datos de la temperatura ambiente o humedad relativa
SensorReading SHT40Sensor::GetSensorData(int opt)
{
    sensors_event_t humidity, temp;

    if (opt == 1)
    {
        sht4.getEvent(nullptr, &temp);

        return {
            deviceId,
            deviceName,
            atmosphereTemperatureMeasurementInfo,
            devicePin,
            connType,
            temp.temperature};
    }
    else if (opt == 2)
    {
        sht4.getEvent(&humidity, nullptr);

        return {
            deviceId,
            deviceName,
            airHumidityMeasurementInfo,
            devicePin,
            connType,
            humidity.relative_humidity};
    }

    return {};
}

// devuelve en un arreglo ambas lecturas del sensor
std::array<SensorReading, 2> SHT40Sensor::GetBothReadings()
{
    sensors_event_t humidity, temp;
    sht4.getEvent(&humidity, &temp);

    SensorReading temperatureReading = {
        deviceId,
        deviceName,
        atmosphereTemperatureMeasurementInfo,
        devicePin,
        connType,
        temp.temperature
    };

    SensorReading humidityReading = {
        deviceId,
        deviceName,
        airHumidityMeasurementInfo,
        devicePin,
        connType,
        humidity.relative_humidity
    };

    return {temperatureReading, humidityReading};
}
