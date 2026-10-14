#include <Arduino.h>
#include "../Sensors/Sensors.h"
#include "../Constants/Constants.h"

// declaración global de instancias únicamente para la clase actual
namespace
{
  Sensors sensors = Sensors();
  unsigned long lastRead = 0;
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  unsigned long start = millis();

  while (!Serial && millis() - start < 3000)
  {
    delay(10);
  }

  sensors.InitializeSensors();

  Serial.print("Sensores inicializados...\n");
  Serial.print("LECTURAS OBTENIDAS \n");

  sensors.PrintReads();
  lastRead = millis();
}

void loop()
{
  unsigned long currentTime = millis();

  if (currentTime - lastRead >= READ_INTERVAL)
  {
    lastRead = currentTime;
    sensors.PrintReads();
  }
}