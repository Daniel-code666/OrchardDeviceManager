#include "Sensors.h"
#include "../Utils/EnumsSwitch.h"
#include "../Devices/DS18B20/DS18B20Sensor.h"
#include "../Devices/BH1750/BH1750Sensor.h"
#include "../Devices/SHT40/SHT40Sensor.h"
#include "../Devices/SoilMoisture/SoilMoistureSensor.h"
#include "../Devices/WaterPump/WaterPump.h"
#include "../Constants/Constants.h"

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

// constructor
Sensors::Sensors()
{}

void Sensors::RegisterIdInstance()
{
    sensorRegistry = {{{ds18b20Sensor.GetDeviceId(), [](FullTelemetry &data)
                        {
                            data.sensorReadings.push_back(ds18b20Sensor.GetSensorData());
                        }},
                       {bh1750Sensor.GetDeviceId(), [](FullTelemetry &data)
                        {
                            data.sensorReadings.push_back(bh1750Sensor.GetSensorData());
                        }},
                       {sht40Sensor.GetDeviceId(), [](FullTelemetry &data)
                        {
                            auto readings = sht40Sensor.GetBothReadings();
                            data.sensorReadings.insert(data.sensorReadings.end(), readings.begin(), readings.end());
                        }},
                       {soilMoistureSensorA.GetDeviceId(), [](FullTelemetry &data)
                        {
                            data.sensorReadings.push_back(soilMoistureSensorA.GetSensorData());
                        }},
                       {soilMoistureSensorB.GetDeviceId(), [](FullTelemetry &data)
                        {
                            data.sensorReadings.push_back(soilMoistureSensorB.GetSensorData());
                        }}}};

    actuatorRegistry = {{{waterPump.GetDeviceId(), [](FullTelemetry &data)
                          {
                              data.actuatorsData.push_back(waterPump.GetActuatorData());
                          }}}};
}

// inicializa los sensores
void Sensors::InitializeSensors()
{
    InitializeWire();

    ds18b20Sensor.Init();
    bh1750Sensor.Init();
    sht40Sensor.Init();
    waterPump.Init();

    RegisterIdInstance();
}

// establece el estado para el funcionamiento de la bomba, sin estado = automático, 1 encendida, 0 apagada
bool Sensors::SetWaterPumpState(std::optional<bool> state)
{
    // si state tiene valor quiere decir que la bomba está siendo accionada de manera manual, automático de lo contrario
    if (state.has_value())
        waterPump.SetMode(1);
    else
        waterPump.SetMode(2);

    AddedSoilSensorsReadings totalSoilData = SumSoilReadings();

    // el valor de state puede ser null
    return waterPump.SetState(totalSoilData.AddedPercentage, totalSoilData.AddedDrySoilValues, totalSoilData.AddedWetSoilValues, state);
}

// obtiene los valores de los sensores, opt = true imprime en consola, false lo contrario
FullTelemetry Sensors::GetDataFromSensors(bool opt)
{
    FullTelemetry fullTelemetry;
    SensorReading ds18b20Reading = ds18b20Sensor.GetSensorData();
    SensorReading bh1750Reading = bh1750Sensor.GetSensorData();
    std::array<SensorReading, 2> sht40Readings = sht40Sensor.GetBothReadings();
    SensorReading soilMoistureSensorAReading = soilMoistureSensorA.GetSensorData();
    SensorReading soilMoistureSensorBReading = soilMoistureSensorB.GetSensorData();
    ActuatorData actuatorData = waterPump.GetActuatorData();

    fullTelemetry.actuatorsData.push_back(actuatorData);

    fullTelemetry.sensorReadings = {
        ds18b20Reading,
        bh1750Reading,
        soilMoistureSensorAReading,
        soilMoistureSensorBReading};

    fullTelemetry.sensorReadings.insert(fullTelemetry.sensorReadings.end(), sht40Readings.begin(), sht40Readings.end());

    if (opt)
    {
        PrintSensorData(fullTelemetry.sensorReadings);
        PrintActuatorData(fullTelemetry.actuatorsData);
    }

    return fullTelemetry;
}

// inicializa WIRE para el bh1750
void Sensors::InitializeWire()
{
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
}

// imprime los datos de todos los sensores incluidos en el arreglo
void Sensors::PrintSensorData(std::vector<SensorReading> sensors)
{
    for (int i = 0; i < sensors.size(); i++)
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

// imprime en consola los datos de los actuadores existentes
void Sensors::PrintActuatorData(std::vector<ActuatorData> actuatorsData)
{
    for (int i = 0; i < actuatorsData.size(); i++)
    {
        Serial.print("\n");
        Serial.print(actuatorsData[i].actuatorId);
        Serial.print("|");
        Serial.print(actuatorsData[i].actuatorName.c_str());
        Serial.print("|");
        Serial.print(actuatorsData[i].actuatorPin);
        Serial.print("|");
        Serial.print(enumsSwitch.ConnectionTypeName(actuatorsData[i].connType).c_str());
        Serial.print("|");
        Serial.print(enumsSwitch.PumpControlModeName(actuatorsData[i].mode).c_str());
        Serial.print("|");
        Serial.print(actuatorsData[i].value);
        Serial.print("\n");
    }
}

// unifica las lecturas del sensor de suelo
AddedSoilSensorsReadings Sensors::SumSoilReadings()
{
    SensorReading soilMoistureSensorAReading = soilMoistureSensorA.GetSensorData();
    SensorReading soilMoistureSensorBReading = soilMoistureSensorB.GetSensorData();

    return {
        soilMoistureSensorA.GetDrySoilPercentage() + soilMoistureSensorB.GetDrySoilPercentage(),
        soilMoistureSensorA.GetWetSoilPercentage() + soilMoistureSensorB.GetWetSoilPercentage(),
        soilMoistureSensorAReading.value + soilMoistureSensorBReading.value};
}

// establece el modo de la bomba de agua: 1 manual, 2 automático
bool Sensors::SetWaterPumpMode(int opt)
{
    return waterPump.SetMode(opt);
}

bool Sensors::IsWaterPumpActive()
{
    return waterPump.GetState();
}

FullTelemetry Sensors::GetSensorOrActuatorDataById(DeviceIdByType &deviceIdByType)
{
    FullTelemetry fullTelemetry;

    for (int id : deviceIdByType.sensorIds)
    {
        for (const auto &device : sensorRegistry)
        {
            if (device.id == id)
            {
                device.read(fullTelemetry);
                break;
            }
        }
    }

    for (int id : deviceIdByType.actuatorIds)
    {
        for (const auto &actuator : actuatorRegistry)
        {
            if (actuator.id == id)
            {
                actuator.read(fullTelemetry);
                break;
            }
        }
    }

    return fullTelemetry;
}
