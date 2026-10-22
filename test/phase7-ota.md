# Pruebas de aceptación — Fase 7

## Alcance implementado

La actualización se realiza completamente desde el ESP32 y un servidor local de
pruebas. No existe dependencia de backend .NET.

- Particiones `app0/app1` y rollback del bootloader.
- Manifiesto firmado con ECDSA P-256 y SHA-256.
- La firma cubre versión, placa, tamaño, hash, URL e identificador de release.
- Descarga incremental de 2 KiB sin almacenar la imagen completa en RAM.
- Verificación SHA-256 durante la escritura de la partición inactiva.
- Rechazo de downgrades, versiones repetidas, placa incorrecta y firmas inválidas.
- Persistencia del estado en NVS y detección de descargas interrumpidas.
- Confirmación después de 10 segundos de arranque local estable.
- Rollback si la versión arrancada no coincide o no supera la validación en 60 segundos.
- Conservación de identidad, Wi-Fi, vinculación y credenciales MQTT.
- La actualización no inicia con la bomba encendida y no ejecuta comandos de
  actuadores mientras descarga.

HTTP sin TLS se admite únicamente porque `ENABLE_LOCAL_OTA_MOCK=1`. La firma
ECDSA sigue verificando autenticidad e integridad. Un transporte productivo debe
usar HTTPS con una CA configurada y retirar esa bandera.

## Preparar una versión nueva

1. Cambiar `FIRMWARE_VERSION` en `src/Constants/FirmwareVersion.h`, por ejemplo
   de `1.0.0` a `1.1.0`.
2. Compilar:

```powershell
platformio run
```

3. Inicializar la clave local una sola vez:

```powershell
node tools/phase7_ota_mock.mjs init --header src/Ota/OtaPublicKey.h
```

La clave privada queda en `.ota-local/keys` y está ignorada por Git. El comando
escribe la clave pública correspondiente en `src/Ota/OtaPublicKey.h`. Si se crea
una pareja nueva, primero hay que cargar por cable un firmware que confíe en esa
nueva clave pública.

4. Empaquetar y firmar el binario, reemplazando la IP por la del equipo:

```powershell
node tools/phase7_ota_mock.mjs package --firmware .pio/build/esp32-c3-devkitm-1/firmware.bin --version 1.1.0 --release-id release-1.1.0 --base-url http://192.168.0.13:8080 --output .ota-local/release-1.1.0
```

5. Servir la release:

```powershell
node tools/phase7_ota_mock.mjs serve --directory .ota-local/release-1.1.0 --port 8080
```

## Iniciar desde el transporte serie local

Pegar una sola línea:

```json
{"type":"ota_update_request","manifestUrl":"http://192.168.0.13:8080/manifest.json"}
```

Debe responder `Resultado de actualizacion OTA: started`. `OTA_STATUS` muestra
el estado y porcentaje. Al finalizar, el ESP32 reinicia, permanece diez segundos
en validación y registra `Firmware OTA confirmado`.

## Casos negativos

- Alterar cualquier campo del manifiesto: `authentication_failed`.
- Alterar `firmware.bin`: la comprobación SHA-256 impide activarlo.
- Usar la misma versión o una inferior: `version_rejected`.
- Usar otra placa: `incompatible_board`.
- Cortar la descarga: el firmware anterior continúa operativo y NVS registra
  `download_interrupted` en el siguiente arranque.
- Hacer fallar la salud del nuevo firmware: el bootloader recupera la partición
  anterior.
- Intentar actualizar con la bomba activa: `unsafe_actuator_state`.

## Pruebas automatizadas

```powershell
node --test test/phase7_ota_mock.test.mjs
```

## Validación física realizada

El 22 de octubre de 2026 se validó con el ESP32-C3 conectado en `COM5`:

- descarga firmada desde `http://192.168.0.13:8080`;
- actualización real de `1.2.0` a `1.3.0`;
- escritura y arranque desde la segunda partición;
- estado inicial del bootloader `ESP_OTA_IMG_PENDING_VERIFY`;
- restauración del mismo `DeviceId`, Wi-Fi, vinculación y versión 2 de las
  credenciales MQTT;
- inicialización de sensores y bomba apagada;
- reconexión y suscripción correcta a RabbitMQ;
- confirmación explícita después de la ventana de estabilidad;
- rechazo posterior del mismo manifiesto con `version_rejected`.

La prueba también permitió corregir dos detalles específicos del hardware:

- el orden de vida entre `WiFiClient` y `HTTPClient` durante la lectura del
  manifiesto;
- la confirmación automática temprana de Arduino-ESP32, aplazada mediante
  `verifyRollbackLater()` para que la realice `FirmwareUpdateManager`.

