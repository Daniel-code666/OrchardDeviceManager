# Pruebas de aceptación — arranque de fábrica y persistencia

Estas pruebas se ejecutan con un ESP32-C3 conectado. No imprimen SSID, claves Wi-Fi ni credenciales MQTT.

## 1. Restablecimiento completo

1. Cerrar cualquier monitor serial que esté usando el puerto.
2. Listar los equipos: `.\tools\huerta-device.ps1 list`.
3. Ejecutar `.\tools\huerta-device.ps1 factory-reset -Serial <SERIAL_USB>`.
4. Abrir el monitor con `.\tools\huerta-device.ps1 monitor -Serial <SERIAL_USB>`.
5. Comprobar `Configuracion NVS ausente o invalida. Creando estado de fabrica` y
   `Estado de fabrica listo`.
6. Comprobar que aparece inmediatamente la red `Huerta-<SUFIJO>` y que aún no se
   genera una solicitud de vinculación ni se intenta conectar a MQTT.

`ConfigurationManager::Save()` solo devuelve éxito después de cerrar NVS, abrirla de nuevo y comparar todos los campos leídos con los escritos.

## 2. Wi-Fi antes de la vinculación

1. Entrar a `http://192.168.4.1`, autenticarse y enviar credenciales Wi-Fi válidas.
2. Comprobar `Credenciales Wi-Fi verificadas y guardadas en NVS` y `Conectado | IP:`.
3. Solo después de esos mensajes debe aparecer `PROVISIONING_REQUEST`.
4. Reiniciar el ESP32-C3 y comprobar `Configuracion cargada desde NVS`, la misma
   identidad y la reconexión con las credenciales guardadas.

## 3. Credenciales MQTT después de vincular

1. Aprobar la solicitud local de vinculación.
2. Comprobar que se genera `MQTT_CREDENTIALS_REQUEST` con versión aplicada `0`.
3. Comprobar que el firmware aún no intenta conectarse al broker.
4. Entregar un sobre válido con credenciales individuales y el `mqttClientId`
   indicado por `DEVICE_INFO`.
5. Comprobar que las credenciales se prueban, se guardan y únicamente entonces
   aparece `Conectado al broker`.

Así se verifica que Wi-Fi y MQTT ya no dependen de `Secrets.h` ni de datos
precargados durante el primer arranque.

## 4. Reconexión Wi-Fi y MQTT

1. Con el dispositivo conectado, reiniciar el punto de acceso o interrumpir temporalmente su señal.
2. Comprobar el evento `Desconexion WiFi`.
3. Restaurar la red y comprobar `Conectado | IP:`.
4. Esperar el siguiente intento MQTT (máximo aproximado de 5 segundos después de recuperar Wi-Fi) y comprobar `Conectado al broker` y la suscripción al topic `Actions` con resultado `OK`.
5. Comprobar que vuelve a publicarse telemetría y que una acción recibe respuesta.

## 5. Conservación de identidad al borrar Wi-Fi

La llamada `ConfigurationManager::ClearWifi()` guarda SSID y contraseña vacíos, pero conserva `DeviceId` y todos los parámetros MQTT. En el siguiente arranque la configuración puede leerse desde NVS, aunque `MqttCommunication::Init()` la rechaza hasta que `UpdateWifi()` guarde credenciales válidas.
