# Fase 14 — Validación integral con dos ESP32-C3

## Estado y alcance

La infraestructura de validación está implementada para firmware `1.8.0`. Esta fase
no agrega backend .NET ni modifica los contratos del ESP. Su cierre definitivo exige
ejecutar la matriz sobre dos placas físicas.

Las evidencias se guardan en `.phase14-evidence/`, carpeta excluida de Git. Ningún
archivo debe contener contraseñas, tokens, claves locales ni etiquetas de
autenticación.

## 1. Preparar los binarios

```powershell
platformio run -e esp32-c3-devkitm-1
platformio run -e esp32-c3-devkitm-1-production
.\tools\huerta-device.ps1 list
```

Registrar los dos `DeviceId` y `HardwareId`. Deben ser distintos. Cargar inicialmente
el perfil de desarrollo para usar los servicios HTTP locales; repetir las pruebas
negativas aplicables con producción y una CA real antes de una liberación.

## 2. Iniciar captura MQTT

Usar un usuario RabbitMQ de observación con lectura sobre los topics de ambos equipos:

```powershell
python -m pip install paho-mqtt
```

```powershell
python tools\phase14_mqtt_observer.py `
  --host 192.168.0.13 --port 1884 `
  --user observador-huerta --password CLAVE_OBSERVADOR `
  --output .\.phase14-evidence\mqtt.jsonl
```

El observador captura únicamente telemetría, inventario, respuestas de acciones y
estados de control. Los campos sensibles conocidos se reemplazan por `[REDACTED]`.

## 3. Alta completa de A y B

Para cada placa:

1. Ejecutar `factory-reset` seleccionando su USB serial.
2. Iniciar una instancia independiente del mock de fase 9, con token, puerto,
   usuario MQTT y archivo de clave distintos.
3. Abrir la página del mock en Chrome o Edge.
4. Conectar, identificar y enviar una sola configuración.
5. Esperar `device_configuration_result` exitoso con Wi-Fi, claim y MQTT completos.
6. Pulsar **Descargar evidencia** y guardar como `web-a.json` o `web-b.json`.

No se debe abrir monitor serial ni copiar claims o sobres MQTT durante este flujo.

## 4. Mensajería e aislamiento

Para cada equipo:

- esperar inventario y telemetría en su topic individual;
- enviar `SetWaterPumpState` con un `requestId` único y verificar la misma correlación
  en `actions-response`;
- comprobar que A y B nunca publican usando el `DeviceId` del otro;
- publicar en el topic de A una acción cuyo `targetDeviceId` sea B: A debe rechazarla
  y no accionar la bomba;
- enviar a A una OTA autenticada con la clave de B: debe devolver
  `authentication_failed` y conservar el firmware.

## 5. OTA y persistencia

Crear una versión superior siguiendo `phase12-13-production-remote-ota.md`. Actualizar
A y B por separado y observar para cada `requestId`:

```text
Downloading -> PendingValidation -> Confirmed
```

Después del reinicio deben conservarse identidad, propiedad, Wi-Fi, versión MQTT e
inventario. Cortar energía durante otra descarga debe conservar la imagen anterior y
producir un fallo estable al reconectar.

## 6. Recuperación y operación local

- desconectar Wi-Fi y RabbitMQ; sensores y automatización local deben continuar;
- recuperar Wi-Fi desde Web Serial sin borrar claim ni MQTT;
- revocar MQTT de A, rotar a una versión superior y confirmar que B no se interrumpe;
- ejecutar unlink autenticado en A, volver a vincularlo y confirmar una clave nueva;
- repetir credenciales erróneas, replay, destino cruzado, bomba activa y manifiesto
  alterado según las fases anteriores;
- mantener una prueba prolongada de reconexión durante 24 horas antes de liberar.

## 7. Generar el informe verificable

Cuando el observador haya capturado inventario, telemetría, una respuesta de acción y
OTA `Confirmed` para ambos equipos:

```powershell
node tools\phase14_evidence.mjs `
  --web-a .\.phase14-evidence\web-a.json `
  --web-b .\.phase14-evidence\web-b.json `
  --mqtt-log .\.phase14-evidence\mqtt.jsonl `
  --output .\.phase14-evidence\phase14-report.md
```

El validador:

- rechaza capturas Web Serial con campos sensibles;
- exige identidades físicas diferentes;
- exige configuración completa de ambos equipos;
- valida inventario, telemetría, acción correlacionada y OTA confirmada por equipo;
- calcula SHA-256 de los binarios de desarrollo y producción;
- busca marcadores de comandos manuales sensibles en el binario productivo;
- produce un informe Markdown y otro JSON sin requerir .NET.

Un resultado `APROBADO` cubre las comprobaciones automatizables. El cierre de la fase
requiere además marcar como aprobadas las interrupciones, aislamiento, recuperación y
prueba prolongada descritas arriba.
