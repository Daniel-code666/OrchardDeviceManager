#ifndef WATER_PUMP_H
#define WATER_PUMP_H

#include "Objects/Device.h"
#include "Objects/ActuatorData.h"

class WaterPump : public Device
{
    public:
        explicit WaterPump(int id, int pin, std::string actuatorName);

        void Init() override;
        bool GetState();
        bool SetState(bool state, bool manualSet, bool autoSet, int drySoilValue, int wetSoilValue);
        ActuatorData GetActuatorData();
};

#endif