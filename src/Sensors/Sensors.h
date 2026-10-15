#ifndef SENSORS_H
#define SENSORS_H

#include <vector>
#include "../Objects/SensorReading.h"
#include "../Objects/ActuatorData.h"

class Sensors {
    public:
        void InitializeSensors();
        void GetDataFromSensors(bool opt);
    private:
        void InitializeWire();
        void PrintSensorData(std::vector<SensorReading> sensors);
        void PrintActuatorData(ActuatorData actuatorData);
};

#endif