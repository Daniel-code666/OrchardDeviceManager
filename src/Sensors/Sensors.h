#ifndef SENSORS_H
#define SENSORS_H

#include <vector>
#include <optional>
#include <Wire.h>
#include <vector>
#include <array>
#include "../Objects/SensorReading.h"
#include "../Objects/ActuatorData.h"
#include "../Objects/AddedSoilSensorsReadings.h"
#include "../Objects/FullTelemetry.h"
#include "../Objects/DeviceIdByType.h"
#include "../Objects/DeviceRegistryEntry.h"
#include "../Objects/WaterPumpSetMode.h"

class Sensors {
    public:
        explicit Sensors();

        std::vector<SensorRegistryEntry> sensorRegistry;
        std::vector<ActuatorRegistryEntry> actuatorRegistry;

        void InitializeSensors();
        FullTelemetry GetDataFromSensors(bool opt);
        bool SetWaterPumpState(std::vector<WaterPumpSetMode> data);
        bool SetWaterPumpMode(std::vector<WaterPumpSetMode> data);
        bool IsWaterPumpActive();
        FullTelemetry GetSensorOrActuatorDataById(DeviceIdByType& deviceIdByType);
    private:
        void InitializeWire();
        void PrintSensorData(std::vector<SensorReading> sensors);
        void PrintActuatorData(std::vector<ActuatorData> actuatorData);
        AddedSoilSensorsReadings SumSoilReadings();
        void RegisterIdInstance();
};

#endif
