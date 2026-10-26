# Pruebas — Fases 10 y 11

## Alcance

El firmware `1.7.0` completa el bootstrap administrado iniciado en las fases 8 y 9:

```text
Web Serial -> Wi-Fi -> claim -> sobre MQTT -> prueba RabbitMQ -> resultado final
```

Después de conectarse al broker publica un inventario estable y retenido en:

```text
gardens/{deviceId}/inventory
```

El mock no crea usuarios en RabbitMQ. Para estas pruebas se le entregan credenciales
individuales que ya existan en el broker. Esa creación corresponderá al backend futuro.

## 1. Iniciar el mock con las credenciales de prueba

Desde el directorio del firmware:

```powershell
node .\tools\phase9_managed_provisioning_mock.mjs `
  --port 8090 `
  --token huerta-local-token-2026 `
  --user-id usuario-web-local `
  --garden-id huerta-web-local `
  --mqtt-host 192.168.0.13 `
  --mqtt-port 1884 `
  --mqtt-user device-huerta-c3-XXXXXXXXXXXX `
  --mqtt-password CONTRASENA_MQTT `
  --credential-version 1
```

Sustituir IP, usuario y contraseña por valores reales. El `clientId` no se escribe:
el mock usa automáticamente el que registró el propio ESP durante el claim.

Para ejercitar el polling se pueden agregar:

```text
--pending-polls 2 --credential-pending-polls 2
```

## 2. Ejecutar el alta desde Web Serial

Abrir `http://localhost:8090/` en Chrome o Edge y ejecutar el mismo bootstrap de las
fases 8 y 9. La URL enviada al ESP debe usar la IPv4 LAN del PC, no `localhost`.

Además de los estados de Wi-Fi y claim deben aparecer:

```text
mqtt_credentials / in_progress
mqtt / in_progress
mqtt / completed
```

El resultado final esperado es una sola línea JSON equivalente a:

```json
{
  "type": "device_configuration_result",
  "contractVersion": 1,
  "requestId": "el-request-id-del-bootstrap",
  "success": true,
  "deviceId": "huerta-c3-XXXXXXXXXXXX",
  "firmwareVersion": "1.7.0",
  "provisioningState": "provisioned",
  "wifiConnected": true,
  "mqttConnected": true,
  "mqttCredentialVersion": 1,
  "failureCode": null
}
```

Ni la página ni los eventos del ESP deben mostrar `bootstrapKey`, usuario MQTT,
contraseña MQTT o el contenido descifrado. Después del resultado final, el token se
elimina de NVS. El mock permite repetir exactamente la misma solicitud si se perdió
la respuesta, pero rechaza usar el token para una solicitud diferente.

## 3. Verificar inventario

En RabbitMQ, enlazar una cola de diagnóstico al exchange MQTT con la routing key:

```text
gardens.huerta-c3-XXXXXXXXXXXX.inventory
```

Debe recibirse un mensaje retenido `device_inventory` con este orden lógico:

1. DS18B20, ID 1.
2. BH1750, ID 2.
3. SHT40 temperatura, ID 3.
4. SHT40 humedad, ID 3.
5. Soil Moisture 1, ID 4.
6. Soil Moisture 2, ID 5.
7. Bomba de agua, ID 6.

Un sensor ausente permanece en la lista con `available: false`. La bomba no publica
campos de unidad ni tipo de medición. El inventario se publica nuevamente solo si
cambia su contenido.

En un build de desarrollo también puede enviarse `DEVICE_INVENTORY` por Serial para
inspeccionar el mismo documento entre `DEVICE_INVENTORY_BEGIN/END`.

## 4. Verificar acciones correlacionadas

Publicar en `gardens/{deviceId}/actions`:

```json
{
  "type": "device_action",
  "contractVersion": 1,
  "requestId": "action-pump-001",
  "command": "SetWaterPumpState",
  "params": [{ "deviceId": 6, "state": true }]
}
```

La respuesta en `gardens/{deviceId}/actions-response` debe incluir:

```json
{
  "type": "device_action_result",
  "contractVersion": 1,
  "requestId": "action-pump-001",
  "deviceId": "huerta-c3-XXXXXXXXXXXX",
  "command": "SetWaterPumpState",
  "success": true
}
```

`deviceType` ya no es obligatorio. Si el payload incluye un `deviceId` raíz distinto
al del ESP, el mensaje se descarta aunque haya llegado al topic correcto.

## 5. Casos negativos

- contraseña MQTT incorrecta: rollback y resultado final con
  `mqtt_connection_failed`;
- etiqueta, nonce, AAD o `requestId` alterado: no cambia la configuración activa;
- versión igual o inferior: `stale_version`;
- token consumido usado con otra solicitud: HTTP `410`;
- sensor desconectado al arrancar: permanece inventariado con `available: false`;
- acción con otro `deviceId`: no se ejecuta ni genera respuesta de éxito.

## 6. Validaciones automatizadas

```powershell
node --test .\test\phase5_credentials_mock.test.mjs .\test\phase9_managed_provisioning_mock.test.mjs
platformio run
```

