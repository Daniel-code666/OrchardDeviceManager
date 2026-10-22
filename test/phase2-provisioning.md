# Pruebas de aceptación — Fase 2

Se requiere un ESP32-C3 conectado por USB para observar el código de acceso efímero. Las pruebas no requieren modificar topics MQTT ni servicios backend.

## 1. Equipo sin Wi-Fi configurado

1. Ejecutar `ConfigurationManager::ClearWifi()` una sola vez desde un firmware de prueba y reiniciar.
2. Comprobar que aparece `Modo de configuracion local activo durante 5 minutos`.
3. Conectarse a la red `Huerta-*` con la clave temporal mostrada por serie.
4. Abrir `http://192.168.4.1` y autenticarse con usuario `admin` y la misma clave temporal.
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

## 5. Límite de exposición

1. Abrir el modo local y no enviar credenciales.
2. Comprobar que a los 5 minutos aparece `Ventana de configuracion finalizada` y desaparece el AP.
3. Comprobar que no vuelve a abrirse en ese arranque; reiniciar físicamente para iniciar una nueva ventana con otra clave aleatoria.

## 6. Control local durante el aprovisionamiento

Durante todas las pruebas, comprobar que las lecturas continúan cada `READ_INTERVAL` y que el control automático de la bomba no depende del estado del portal, Wi-Fi o MQTT.
