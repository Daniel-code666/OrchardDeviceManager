#pragma once

#include <stdint.h>

constexpr char WIFI_SSID[] = "FamiliaMartinez";
constexpr char WIFI_PASSWORD[] = "Max.72584!";

constexpr char MQTT_HOST[] = "192.168.0.13";
constexpr uint16_t MQTT_PORT = 1884;
constexpr char MQTT_CLIENT_ID[] = "huerta-esp32-01";
constexpr char MQTT_USER[] = "huerta";
constexpr char MQTT_PASSWORD[] = "12345";

// Identidad inicial usada unicamente para sembrar NVS durante la migracion.
// No constituye una credencial ni un mecanismo de autenticacion.
constexpr char DEVICE_ID[] = "huerta-esp32-01";
