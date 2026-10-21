// constantes: pines y valores
#include <cstdint> 
#include <map>

// HUMEDAD SUELO DS18B20
constexpr uint8_t SOIL_1_PIN = 0;
constexpr uint8_t SOIL_2_PIN = 1;

// BOMBA
constexpr uint8_t PUMP_PIN = 3;

// I2C
constexpr uint8_t I2C_SDA_PIN = 5;
constexpr uint8_t I2C_SCL_PIN = 6;

// TEMPERATURA DEL SUELO
constexpr uint8_t SOIL_TEMP_PIN = 7;

// CONSTANTE DE TIEMPO PARA LECTURA
constexpr unsigned long READ_INTERVAL = 10000;

// CONSTANTE PARA REPRESENTAR NO PIN
constexpr uint8_t NO_PIN = 0xFF;