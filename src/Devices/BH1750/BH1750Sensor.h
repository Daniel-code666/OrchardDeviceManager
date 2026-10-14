#ifndef BHT1750_SENSOR_H
#define BHT1750_SENSOR_H

#include "Objects/Device.h"
#include <BH1750.h>

class BH1750Sensor : public Device 
{
    public:
        explicit BH1750Sensor(int id);

        void Init() override;
        SensorReading GetSensorData();
    private:
        const u_int8_t i2cAddress = 0x23;

        BH1750 lightMeter;

        const MeasurementInfo measurementInfo = {
            MeasurementType::Illuminance,
            "Iluminación",
            MeasurementUnits::Lux
        };
};

#endif