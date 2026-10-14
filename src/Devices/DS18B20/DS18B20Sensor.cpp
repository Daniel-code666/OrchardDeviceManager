#include "DS18B20Sensor.h"
#include "Objects/SensorReading.h"
#include "Objects/ConnectionType.h"

DS18B20Sensor::DS18B20Sensor(int id, uint8_t pin) : Device(id, pin, "DS18B20", ConnectionType::OneWire), oneWire(pin), temperatureSensor(&oneWire)
{
}

void DS18B20Sensor::Init()
{
    // Habilitar resistencia pull-up interna
    pinMode(devicePin, INPUT_PULLUP);

    temperatureSensor.begin();

    Serial.print("Sensores DS18B20 detectados: ");
    Serial.println(temperatureSensor.getDeviceCount());
}

SensorReading DS18B20Sensor::GetSensorData()
{
    temperatureSensor.requestTemperatures();

    float temperature = temperatureSensor.getTempCByIndex(0);

    if (temperature == DEVICE_DISCONNECTED_C)
    {
        Serial.println("DS18B20: error de lectura");
    }

    return {
        deviceName,
        measurementInfo,
        devicePin,
        connType,
        temperature
    };
}
