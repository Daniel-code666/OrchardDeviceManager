#ifndef ENUMS_SWITCH_H
#define ENUMS_SWITCH_H

#include <string>
#include "../Objects/MeasurementTypes.h"
#include "../Objects/MeasurementUnits.h"
#include "../Objects/ConnectionType.h"
#include "../Objects/PumpControlMode.h"
#include "../Objects/Topics.h"
#include "../Objects/ActionMessage.h"
#include <vector>
#include "Objects/DeviceType.h"
#include "Objects/DeviceIdByType.h"

class EnumsSwitch
{
    public:
        std::string MeasurementTypesName(MeasurementType measurementType);
        std::string MeasurementUnitsName(MeasurementUnits measurementName);
        std::string ConnectionTypeName(ConnectionType connType);
        std::string PumpControlModeName(PumpControlMode pumpControlMode);
        std::string TopicsName(Topics topic);
        DeviceIdByType OrganizeIdsByType(ActionMessage actionMessage);
};

#endif