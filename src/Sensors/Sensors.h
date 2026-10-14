#ifndef SENSORS_H
#define SENSORS_H

#include <vector>
#include "../Objects/SensorReading.h"

class Sensors {
    public:
        void InitializeSensors();
        void GetDataFromSensors(bool opt);
    private:
        void InitializeWire();
        void PrintData(std::vector<SensorReading> sensors);
};

#endif