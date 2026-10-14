#include "Sensors.h"
#include "../Devices/DS18B20/DS18B20Sensor.h"
#include "../Devices/BH1750/BH1750Sensor.h"
#include "../Constants/Constants.h"
#include "../Utils/EnumsSwitch.h"

#include <Wire.h>
#include <vector>

// declaración global de instancias únicamente para la clase actual
namespace
{
    DS18B20Sensor ds18b20Sensor(1, SOIL_TEMP_PIN);
    BH1750Sensor bh1750Sensor(2);
    EnumsSwitch enumsSwitch = EnumsSwitch();
}

// inicializa los sensores
void Sensors::InitializeSensors()
{
    InitializeWire();

    ds18b20Sensor.Init();
    bh1750Sensor.Init();
}

// obtiene los valores de los sensores, opt = true imprime en consola, false lo contrario
void Sensors::GetDataFromSensors(bool opt)
{
    SensorReading ds18b20Reading = ds18b20Sensor.GetSensorData();
    SensorReading bh1750Reading = bh1750Sensor.GetSensorData();

    if (opt)
    {
        std::vector<SensorReading> sensors;

        sensors.push_back(ds18b20Reading);
        sensors.push_back(bh1750Reading);
        
        PrintData(sensors);
    }
}

void Sensors::InitializeWire()
{
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
}

void Sensors::PrintData(std::vector<SensorReading> sensors)
{
    // imprime los datos de todos los sensores incluidos en el arreglo
    for(int i = 0; i < sensors.size(); i++)
    {
        Serial.print(sensors[i].sensorId);
        Serial.print("|");
        Serial.print(sensors[i].sensorName.c_str());
        Serial.print("|");
        Serial.print(sensors[i].sensorPin);
        Serial.print("|");
        Serial.print(enumsSwitch.ConnectionTypeName(sensors[i].connType).c_str());
        Serial.print("|");
        Serial.print(enumsSwitch.MeasurementTypesName(sensors[i].measurementInfo.type).c_str());
        Serial.print("|");
        Serial.print(enumsSwitch.MeasurementUnitsName(sensors[i].measurementInfo.unit).c_str());
        Serial.print("|");
        Serial.print("\n");
    }
}