# Pruebas de las fases 12 y 13

## Alcance

El firmware `1.8.0` incorpora:

- perfiles separados `esp32-c3-devkitm-1` (desarrollo) y
  `esp32-c3-devkitm-1-production`;
- recuperación de Wi-Fi mediante Web Serial sin borrar claim, propiedad ni
  credenciales MQTT;
- comandos heredados y exposición de secretos deshabilitados en producción;
- órdenes OTA y de recuperación autenticadas en el topic individual;
- estados OTA correlacionados y retenidos en MQTT.

No se utiliza ni se requiere backend .NET.

## Compilación

```powershell
platformio run -e esp32-c3-devkitm-1
platformio run -e esp32-c3-devkitm-1-production
```

El archivo `src/Security/TrustedRootCa.h` se entrega vacío de forma deliberada. Un
firmware productivo puede compilar, pero rechazará HTTPS hasta que el pipeline
inyecte la CA PEM real. Nunca debe habilitarse TLS inseguro en producción.

## Recuperación de Wi-Fi por Web Serial

1. Cargar el perfil de desarrollo o producción y abrir
   `tools/managed-provisioning-web.html` en Chrome o Edge desde un origen permitido.
2. Conectar e identificar el ESP.
3. Cuando el estado sea `provisioned`, la página ocultará URL y token y mostrará
   **Actualizar Wi-Fi**.
4. Enviar la nueva red. La página envía `device_wifi_reconfigure`, prueba la red y
   espera `wifi_recovery: completed`.
5. Reiniciar y comprobar que permanecen el mismo `deviceId`, claim y versión de
   credenciales MQTT.
6. Repetir con una contraseña incorrecta: debe informar fallo y reconectar usando la
   última red válida.

Un dispositivo `unprovisioned` no puede usar el contrato de recuperación para
evitar el bootstrap: debe completar el flujo normal con URL y token.

## Preparación local para OTA remota

Durante un alta de desarrollo, el mock puede conservar la clave individual en un
archivo ignorado por Git. Esta opción existe solo para pruebas y la clave nunca pasa
por el navegador:

```powershell
node tools\phase9_managed_provisioning_mock.mjs --host 0.0.0.0 --port 8090 `
  --token huerta-local-token-2026 --mqtt-host 192.168.0.13 --mqtt-port 1884 `
  --mqtt-user device-local --mqtt-password CLAVE_MQTT `
  --device-key-output .\.ota-local\device-key.json
```

Para probar una actualización real, el ESP debe ejecutar una versión anterior a la
imagen. Por ejemplo, cargar `1.8.0`, cambiar `FirmwareVersion.h` a `1.8.1`, compilar
y empaquetar ese binario con el mismo procedimiento de la fase 7. El `release-id`
de la orden y del manifiesto debe ser idéntico.

El servidor de la fase 7 usa HTTP y, por tanto, solo sirve para el perfil de
desarrollo. El perfil de producción exige HTTPS con la CA configurada.

## Generar y enviar la orden OTA

```powershell
node tools\phase13_ota_command_mock.mjs `
  --device-id huerta-c3-a0f262a61a68 `
  --request-id ota-000000000000000000000001 `
  --command-version 1 `
  --release-id release-1.8.1 `
  --manifest-url http://192.168.0.10:8080/manifest.json `
  --expires-at 2026-10-27T23:00:00Z `
  --local-key-file .\.ota-local\device-key.json
```

Publicar el JSON generado mediante el exchange topic de RabbitMQ con routing key:

```text
gardens.huerta-c3-a0f262a61a68.control
```

Enlazar una cola de observación a:

```text
gardens.*.control-status
```

El ESP publica estados retenidos en
`gardens/{deviceId}/control-status`. Deben aparecer `Downloading`,
`PendingValidation` y, después del reinicio y la ventana de salud, `Confirmed`.

## Pruebas negativas mínimas

- repetir el mismo `requestId` o `commandVersion`: `replay_rejected`;
- modificar un campo después de firmar: `authentication_failed`;
- usar otro `deviceId`: `invalid_contract`;
- usar una fecha vencida: `request_expired`;
- indicar un `releaseId` diferente al manifiesto: `release_mismatch`;
- iniciar con la bomba activa: `unsafe_actuator_state`;
- enviar una acción durante `Downloading`: se descarta y su respuesta lleva
  `success: false`;
- cortar Wi-Fi o energía: permanece la imagen anterior o entra en rollback, y el
  último estado se publica al recuperar MQTT.

Las órdenes `recovery_command` de la fase 6 también pueden publicarse en el topic
`control`; la respuesta correlacionada aparece en `control-status`. En producción
los comandos manuales pegados por Serial deben responder como no admitidos.
