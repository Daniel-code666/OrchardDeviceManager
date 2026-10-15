#ifndef SENSORS_H
#define SENSORS_H

#include <vector>
#include "../Objects/SensorReading.h"
#include "../Objects/ActuatorData.h"
#include "../Objects/AddedSoilSensorsReadings.h"

class Sensors {
    public:
        void InitializeSensors();
        void GetDataFromSensors(bool opt);
        bool SetWaterPumpState(bool state, bool manualSet);
    private:
        void InitializeWire();
        void PrintSensorData(std::vector<SensorReading> sensors);
        void PrintActuatorData(ActuatorData actuatorData);
        AddedSoilSensorsReadings SumSoilReadings();
};

#endif