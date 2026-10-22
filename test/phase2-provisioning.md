# Pruebas de aceptación — Fase 2

Se requiere un ESP32-C3 conectado por USB para observar la clave estable durante
el desarrollo. En producción, esta clave única por dispositivo debe entregarse
mediante una etiqueta o código QR. Las pruebas no requieren modificar topics MQTT
ni servicios backend.

## 1. Equipo sin Wi-Fi configurado

1. Ejecutar `ConfigurationManager::ClearWifi()` una sola vez desde un firmware de prueba y reiniciar.
2. Comprobar que aparece `Modo de configuracion local activo hasta recuperar la conexion Wi-Fi`.
3. Conectarse a la red `Huerta-*` con la clave estable mostrada por serie.
4. Abrir `http://192.168.4.1` y autenticarse con usuario `admin` y la misma clave.
5. Verificar que la interfaz no muestra ni devuelve credenciales previamente almacenadas.

## 2. Validación antes de guardar

1. Introducir un SSID inexistente o una contraseña incorrecta.
2. Consultar `/status` después de 20 segundos.
3. Comprobar que informa del fallo y que la configuración anterior sigue en NVS.
4. Introducir credenciales válidas.
5. Comprobar por serie `Credenciales Wi-Fi verificadas y guardadas en NVS`.

## 3. Persistencia y reconexión

1. Reiniciar el ESP32-C3.
2. Comprobar que carga la configuración desde NVS y se conecta sin abrir el SoftAP.
3. Comprobar reconexión MQTT, suscripción a `Actions`, telemetría y respuesta a una acción.

## 4. Recuperación por cambio de router

1. Apagar la red configurada o cambiar su contraseña.
2. Reiniciar el dispositivo.
3. Comprobar que intenta Wi-Fi durante 30 segundos y después abre `Huerta-*`.
4. Configurar la nueva red y verificar que solo se guarda después de obtener IP.

## 5. Recuperación disponible sin reiniciar

1. Abrir el modo local y no enviar credenciales.
2. Comprobar que el SoftAP continúa visible después de 5 minutos.
3. Comprobar que la clave coincide con la utilizada en aperturas anteriores.
4. Comprobar que el ESP no intenta conectarse nuevamente por su cuenta mientras el portal está activo.
5. Enviar credenciales válidas y comprobar que solo entonces cierra el SoftAP.
6. Volver a interrumpir Wi-Fi y comprobar que el portal reaparece después de 30 segundos sin reiniciar el ESP.

## 6. Control local durante el aprovisionamiento

Durante todas las pruebas, comprobar que las lecturas continúan cada `READ_INTERVAL` y que el control automático de la bomba no depende del estado del portal, Wi-Fi o MQTT.

## 7. Interrupción Wi-Fi controlada por serie

Con `ENABLE_LOCAL_PROVISIONING_MOCK=1`, enviar una línea como esta por el monitor
serie:

```text
WIFI_OUTAGE 60
```

El ESP desconecta temporalmente la estación sin cambiar las credenciales guardadas
en NVS. Después de 30 segundos debe abrir el portal `Huerta-*`; al cumplirse los
60 segundos debe cerrar el portal, restaurar la conexión Wi-Fi y posteriormente
reconectar MQTT. El rango aceptado es de 1 a 3600 segundos. Para observar el
portal debe elegirse un valor superior a 30 segundos. Si se envían credenciales
desde el portal antes de finalizar el tiempo, la interrupción simulada se cancela
y se realiza exclusivamente ese nuevo intento de conexión.
