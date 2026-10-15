#ifndef SHT40_SENSOR_H
#define SHT40_SENSOR_H

#include "Objects/Device.h"
#include "Adafruit_SHT4x.h"
#include <array>

class SHT40Sensor : public Device
{
    public:
        explicit SHT40Sensor(int id);

        const u_int8_t i2cAddress = 0x44;

        void Init() override;
        SensorReading GetSensorData(int opt);
        std::array<SensorReading, 2> GetBothReadings();
    private:
        Adafruit_SHT4x sht4 = Adafruit_SHT4x();

        const MeasurementInfo atmosphereTemperatureMeasurementInfo = {
            MeasurementType::AirTemperature,
            "Temperatura ambiente",
            MeasurementUnits::Celsius
        };

        const MeasurementInfo airHumidityMeasurementInfo = {
            MeasurementType::AirHumidity,
            "Humedad relativa",
            MeasurementUnits::Percentage
        };
};

#endif