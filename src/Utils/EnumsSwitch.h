#ifndef ENUMS_SWITCH_H
#define ENUMS_SWITCH_H

#include <string>
#include "../Objects/MeasurementTypes.h"
#include "../Objects/MeasurementUnits.h"
#include "../Objects/ConnectionType.h"
#include "../Objects/PumpControlMode.h"

class EnumsSwitch
{
    public:
        std::string MeasurementTypesName(MeasurementType measurementType);
        std::string MeasurementUnitsName(MeasurementUnits measurementName);
        std::string ConnectionTypeName(ConnectionType connType);
        std::string PumpControlModeName(PumpControlMode pumpControlMode);
};

#endif