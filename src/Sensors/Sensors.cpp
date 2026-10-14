#include "Sensors.h"
#include "../Devices/DS18B20/DS18B20Sensor.h"
#include "../Constants/Constants.h"

// declaración global de instancias únicamente para la clase actual
namespace
{
    DS18B20Sensor ds18b20Sensor(1, SOIL_TEMP_PIN);
}

// inicializa los sensores
void Sensors::InitializeSensors()
{
    ds18b20Sensor.Init();
}

// imprime los valores en una tabla de cada sensor
void Sensors::PrintReads()
{
    SensorReading ds18b20Reading = ds18b20Sensor.GetSensorData();

    Serial.print(ds18b20Reading.sensorName.c_str());
    Serial.print(" | ");
    Serial.print(ds18b20Reading.value);
    Serial.print(" | ");
}