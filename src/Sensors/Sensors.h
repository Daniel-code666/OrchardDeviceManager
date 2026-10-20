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

class Sensors {
    public:
        explicit Sensors();

        std::vector<DeviceRegistryEntry> sensorRegistry;
        std::vector<DeviceRegistryEntry> actuatorRegistry;

        void InitializeSensors();
        FullTelemetry GetDataFromSensors(bool opt);
        bool SetWaterPumpState(std::optional<bool> state = std::nullopt);
        bool SetWaterPumpMode(int opt);
        FullTelemetry GetSensorOrActuatorDataById(DeviceIdByType& deviceIdByType);
    private:
        void InitializeWire();
        void PrintSensorData(std::vector<SensorReading> sensors);
        void PrintActuatorData(std::vector<ActuatorData> actuatorData);
        AddedSoilSensorsReadings SumSoilReadings();
        void RegisterIdInstance();
};

#endif