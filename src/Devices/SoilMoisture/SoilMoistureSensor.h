#ifndef SOIL_MOISTURE_SENSOR_H
#define SOIL_MOISTURE_SENSOR_H
#include "Objects/Device.h"
#include "Objects/SensorReading.h"

class SoilMoistureSensor : public Device
{
    public:
        explicit SoilMoistureSensor(int id, uint8_t pin, std::string instanceName, int airValue, int waterValue, int drySoilValue, int wetSoilValue);

        void Init() override;
        SensorReading GetSensorData();
    private:
        const int airValue;
        const int waterValue;
        const int drySoilValue;
        const int wetSoilValue;

        MeasurementInfo measurementInfo = {
            MeasurementType::SoilMoisture,
            "Soil moisture",
            MeasurementUnits::Percentage
        };
};

#endif