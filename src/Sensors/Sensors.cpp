#include "Sensors.h"
#include "../Devices/DS18B20/DS18B20Sensor.h"
#include "../Devices/BH1750/BH1750Sensor.h"
#include "../Devices/SHT40/SHT40Sensor.h"
#include "../Devices/SoilMoisture/SoilMoistureSensor.h"
#include "../Devices/WaterPump/WaterPump.h"
#include "../Constants/Constants.h"
#include "../Utils/EnumsSwitch.h"

#include <Wire.h>
#include <vector>

// declaración global de instancias únicamente para la clase actual
namespace
{
    DS18B20Sensor ds18b20Sensor(1, SOIL_TEMP_PIN);
    BH1750Sensor bh1750Sensor(2);
    SHT40Sensor sht40Sensor(3);
    SoilMoistureSensor soilMoistureSensorA(4, SOIL_1_PIN, "Soil Moisture 1", 3009, 1290, 2587, 2250);
    SoilMoistureSensor soilMoistureSensorB(5, SOIL_2_PIN, "Soil Moisture 2", 2909, 900, 2462, 2172);
    WaterPump waterPump(6, PUMP_PIN, "Bomba agua");
    EnumsSwitch enumsSwitch = EnumsSwitch();
}

// inicializa los sensores
void Sensors::InitializeSensors()
{
    InitializeWire();

    ds18b20Sensor.Init();
    bh1750Sensor.Init();
    sht40Sensor.Init();
}

// obtiene los valores de los sensores, opt = true imprime en consola, false lo contrario
void Sensors::GetDataFromSensors(bool opt)
{
    SensorReading ds18b20Reading = ds18b20Sensor.GetSensorData();
    SensorReading bh1750Reading = bh1750Sensor.GetSensorData();
    std::array<SensorReading, 2> sht40Readings = sht40Sensor.GetBothReadings();
    SensorReading soilMoistureSensorAReading = soilMoistureSensorA.GetSensorData();
    SensorReading soilMoistureSensorBReading = soilMoistureSensorB.GetSensorData();
    ActuatorData actuatorData = waterPump.GetActuatorData();

    if (opt)
    {
        std::vector<SensorReading> sensors;

        sensors.push_back(ds18b20Reading);
        sensors.push_back(bh1750Reading);
        sensors.push_back(sht40Readings[0]);
        sensors.push_back(sht40Readings[1]);
        sensors.push_back(soilMoistureSensorAReading);
        sensors.push_back(soilMoistureSensorBReading);
        
        PrintSensorData(sensors);

        PrintActuatorData(actuatorData);
    }
}

// inicializa WIRE para el bh1750
void Sensors::InitializeWire()
{
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
}

// imprime los datos de todos los sensores incluidos en el arreglo
void Sensors::PrintSensorData(std::vector<SensorReading> sensors)
{
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
        Serial.print(sensors[i].value);
        Serial.print("|");
        Serial.print("\n");
    }
}

void Sensors::PrintActuatorData(ActuatorData actuatorData)
{
    Serial.print("\n");
    Serial.print(actuatorData.actuatorId);
    Serial.print("|");
    Serial.print(actuatorData.actuatorName.c_str());
    Serial.print("|");
    Serial.print(actuatorData.actuatorPin);
    Serial.print("|");
    Serial.print(enumsSwitch.ConnectionTypeName(actuatorData.connType).c_str());
    Serial.print("|");
    Serial.print(actuatorData.value);
    Serial.print("\n");
}