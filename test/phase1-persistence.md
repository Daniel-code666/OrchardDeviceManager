# Pruebas de aceptación — Fase 1

Estas pruebas se ejecutan con un ESP32-C3 conectado. No imprimen SSID, claves Wi-Fi ni credenciales MQTT.

## 1. Escritura y lectura inmediata

1. Mantener temporalmente `ENABLE_DEVELOPMENT_BOOTSTRAP=1` en `platformio.ini`.
2. Cargar el firmware: `platformio run --target upload`.
3. Abrir el monitor: `platformio device monitor --baud 115200`.
4. En un dispositivo sin el namespace `huerta_cfg`, comprobar este mensaje:
   `Configuracion migrada y verificada mediante lectura desde NVS`.

`ConfigurationManager::Save()` solo devuelve éxito después de cerrar NVS, abrirla de nuevo y comparar todos los campos leídos con los escritos.

## 2. Persistencia después de reiniciar

1. Reiniciar el ESP32-C3 sin volver a cargar el firmware.
2. Comprobar que aparece `Configuracion cargada desde NVS` y no aparece `Iniciando migracion de desarrollo`.
3. Comprobar que la versión indicada es `1` y que el `DeviceId` permanece igual.

## 3. Firmware sin credenciales de desarrollo

1. Después de completar las dos pruebas anteriores, eliminar de `platformio.ini` la bandera `ENABLE_DEVELOPMENT_BOOTSTRAP=1`.
2. Compilar y cargar de nuevo el firmware.
3. Reiniciar y comprobar que la configuración continúa cargándose desde NVS.

Así se verifica que Wi-Fi y MQTT ya no dependen de `Secrets.h` durante la operación normal.

## 4. Reconexión Wi-Fi y MQTT

1. Con el dispositivo conectado, reiniciar el punto de acceso o interrumpir temporalmente su señal.
2. Comprobar el evento `Desconexion WiFi`.
3. Restaurar la red y comprobar `Conectado | IP:`.
4. Esperar el siguiente intento MQTT (máximo aproximado de 5 segundos después de recuperar Wi-Fi) y comprobar `Conectado al broker` y la suscripción al topic `Actions` con resultado `OK`.
5. Comprobar que vuelve a publicarse telemetría y que una acción recibe respuesta.

## 5. Conservación de identidad al borrar Wi-Fi

La llamada `ConfigurationManager::ClearWifi()` guarda SSID y contraseña vacíos, pero conserva `DeviceId` y todos los parámetros MQTT. En el siguiente arranque la configuración puede leerse desde NVS, aunque `MqttCommunication::Init()` la rechaza hasta que `UpdateWifi()` guarde credenciales válidas.
