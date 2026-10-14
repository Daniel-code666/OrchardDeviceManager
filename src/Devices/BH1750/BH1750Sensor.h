#ifndef BHT1750_SENSOR_H
#define BHT1750_SENSOR_H

#include "Objects/Device.h"

class BH1750Sensor : public Device 
{
    public:
        explicit BH1750Sensor(int id, uint8_t pin);

        void Init() override;
        SensorReading GetSensorData();
    private:
        const MeasurementInfo measurementInfo = {
            MeasurementType::Illuminance,
            "Iluminación",
            "°C"
        };
};

#endif