#ifndef DS18B20_SENSOR_H
#define DS18B20_SENSOR_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Objects/SensorReading.h"
#include "Objects/MeasurementInfo.h"
#include "Objects/Device.h"

class DS18B20Sensor : public Device
{
    public:
        explicit DS18B20Sensor(int id, uint8_t pin);

        void Init() override;
        SensorReading GetSensorData();
    private:
        // instancia a biblioteca que gestiona el sensor
        OneWire oneWire;
        DallasTemperature temperatureSensor;

        const MeasurementInfo measurementInfo = {
            MeasurementType::SoilTemperature,
            "Temperatura del suelo",
            MeasurementUnits::Celcius
        };
};

#endif
