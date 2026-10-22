# Pruebas de aceptación — Fase 5

## Flujo implementado

1. El dispositivo debe estar `Provisioned` por la Fase 4.
2. El ESP32 genera y conserva en NVS una clave AES-256 individual.
3. El transporte local de desarrollo muestra la clave y una solicitud con
   `DeviceId`, `RequestId` y versión aplicada.
4. El mock cifra las credenciales con AES-256-GCM. Los metadatos de solicitud se
   autentican como AAD y no pueden modificarse.
5. El ESP32 valida y prueba la configuración contra RabbitMQ durante 20 segundos.
6. Solo después de conectarse guarda la configuración y confirma su versión.
7. Si falla el broker, la autenticación o NVS, recupera la configuración anterior.

La clave se muestra por serie únicamente porque `ENABLE_LOCAL_PROVISIONING_MOCK`
está activo. Un transporte definitivo debe obtenerla mediante un canal seguro y
no exponerla en logs.

## Obtener la solicitud

Enviar `MQTT_CREDENTIALS_REQUEST` por el monitor serie. Copiar el JSON de la
solicitud a `mqtt-request.json` y conservar aparte el valor comprendido entre
`MQTT_CREDENTIALS_LOCAL_KEY_BEGIN` y `MQTT_CREDENTIALS_LOCAL_KEY_END`.

## Crear un paquete cifrado

```powershell
node tools/phase5_credentials_mock.mjs --request-file mqtt-request.json --local-key CLAVE_LOCAL --host 192.168.0.13 --port 1884 --client-id mqtt-huerta-esp32-01 --user device-huerta-esp32-01 --password CLAVE_MQTT --version 1
```

Pegar la línea JSON resultante en el monitor. El resultado esperado es
`accepted`, seguido por la prueba de conexión y la confirmación de la versión.

## Casos negativos

- Añadir `--invalid-authentication`: debe devolver `authentication_failed`.
- Usar una contraseña MQTT incorrecta: tras 20 segundos debe restaurar las
  credenciales anteriores.
- Reenviar el mismo paquete después de confirmarlo: debe devolver
  `invalid_state`.
- Enviar una versión igual o menor a la aplicada: debe devolver `stale_version`.
- Reiniciar después de una actualización correcta: debe usar las credenciales
  individuales persistidas.

## Rotación controlada

Enviar `MQTT_CREDENTIALS_REQUEST_NEW` para crear otra solicitud después de haber
aplicado una versión. La nueva versión debe ser estrictamente mayor. La revocación
y desvinculación siguen fuera del alcance hasta la Fase 6.

