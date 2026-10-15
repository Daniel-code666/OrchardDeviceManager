#include <string>
#include "EnumsSwitch.h"

std::string EnumsSwitch::MeasurementTypesName(MeasurementType measurementType)
{
    switch (measurementType)
    {
        case MeasurementType::SoilTemperature: return "SoilTemperature";
        case MeasurementType::AirTemperature: return "AirTemperature";
        case MeasurementType::AirHumidity: return "AirHumidity";
        case MeasurementType::Illuminance: return "Illuminance";
        case MeasurementType::SoilMoisture: return "SoulMoisture";
        default: return "Desconocido";
    }
}

std::string EnumsSwitch::MeasurementUnitsName(MeasurementUnits measurementUnits)
{
    switch (measurementUnits)
    {
        case MeasurementUnits::Celcius: return "Celcius";
        case MeasurementUnits::Lux: return "Lux";
        case MeasurementUnits::Percentage: return "Percentage";
        default: return "Desconocido";
    }
}

std::string EnumsSwitch::ConnectionTypeName(ConnectionType connType)
{
    switch (connType)
    {
        case ConnectionType::Analog: return "Analog";
        case ConnectionType::Digital: return "Digital";
        case ConnectionType::I2C: return "I2C";
        case ConnectionType::OneWire: return "OneWire";
        default: return "Desconocido";
    }
}