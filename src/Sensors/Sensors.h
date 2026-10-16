#ifndef SENSORS_H
#define SENSORS_H

#include <vector>
#include "../Objects/SensorReading.h"
#include "../Objects/ActuatorData.h"
#include "../Objects/AddedSoilSensorsReadings.h"
#include <optional>

class Sensors {
    public:
        void InitializeSensors();
        void GetDataFromSensors(bool opt);
        bool SetWaterPumpState(std::optional<bool> state = std::nullopt);
        bool SetWaterPumpMode(int opt);
    private:
        void InitializeWire();
        void PrintSensorData(std::vector<SensorReading> sensors);
        void PrintActuatorData(ActuatorData actuatorData);
        AddedSoilSensorsReadings SumSoilReadings();
};

#endif