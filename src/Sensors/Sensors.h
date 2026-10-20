#ifndef SENSORS_H
#define SENSORS_H

#include <vector>
#include "../Objects/SensorReading.h"
#include "../Objects/ActuatorData.h"
#include "../Objects/AddedSoilSensorsReadings.h"
#include <optional>
#include "../Objects/FullTelemetry.h"
#include "../Objects/DeviceIdByType.h"

class Sensors {
    public:
        void InitializeSensors();
        FullTelemetry GetDataFromSensors(bool opt);
        bool SetWaterPumpState(std::optional<bool> state = std::nullopt);
        bool SetWaterPumpMode(int opt);
        FullTelemetry GetSensorOrActuatorDataById(DeviceIdByType deviceIdByType);
    private:
        void InitializeWire();
        void PrintSensorData(std::vector<SensorReading> sensors);
        void PrintActuatorData(std::vector<ActuatorData> actuatorData);
        AddedSoilSensorsReadings SumSoilReadings();
};

#endif