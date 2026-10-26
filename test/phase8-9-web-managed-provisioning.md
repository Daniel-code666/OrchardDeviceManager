# Pruebas — Fases 8 y 9 (Web Serial y claim administrado)

## Alcance implementado

El contrato se introdujo en el firmware `1.6.0` y continúa disponible en las
versiones posteriores. Admite dos mensajes JSON por USB/Web Serial:

- `device_identify`, que devuelve identidad, versión y estado sin datos sensibles;
- `device_bootstrap`, que recibe una sola vez Wi‑Fi, URL del servicio y token temporal.

Después del bootstrap, el ESP prueba el Wi‑Fi antes de guardarlo, registra su claim
contra el mock, valida la respuesta autenticada y alcanza `provisioned`. La entrega
automática de credenciales MQTT pertenece a la fase 10 y no forma parte de esta prueba.

## 1. Compilar y cargar

```powershell
.\tools\huerta-device.ps1 upload -Serial 'SERIAL_USB_DEL_ESP'
```

Para una prueba completamente nueva:

```powershell
.\tools\huerta-device.ps1 factory-reset -Serial 'SERIAL_USB_DEL_ESP'
.\tools\huerta-device.ps1 upload -Serial 'SERIAL_USB_DEL_ESP'
```

## 2. Iniciar el servicio simulado

En una terminal del proyecto:

```powershell
node .\tools\phase9_managed_provisioning_mock.mjs --port 8090 --token huerta-local-token-2026 --user-id usuario-web-local --garden-id huerta-web-local --pending-polls 2
```

El mock muestra la URL de la interfaz. Abrir en Chrome o Edge:

```text
http://localhost:8090/
```

`localhost` es válido para Web Serial. En el formulario, en cambio, la URL del
servicio debe usar la IPv4 del PC en la red Wi‑Fi, por ejemplo:

```text
http://192.168.0.13:8090
```

Se obtiene con `ipconfig`. No usar `localhost` ni `127.0.0.1`, porque desde el ESP
esas direcciones apuntan al propio microcontrolador.

HTTP se admite aquí únicamente por `ENABLE_LOCAL_PROVISIONING_MOCK=1`. El mock puede
levantarse con `--cert certificado.pem --key clave.pem` para probar HTTPS local; el
firmware de desarrollo acepta ese certificado autofirmado. Producción no acepta HTTP
ni TLS sin una CA configurada.

## 3. Ejecutar desde la página

1. Pulsar **Conectar dispositivo** y elegir el puerto del ESP.
2. Pulsar **Identificar**. Debe aparecer `device_identify_result` con `deviceId`,
   `hardwareId`, `mqttClientId`, placa, firmware y estados actuales.
3. Completar SSID, contraseña, URL LAN del mock y el token usado al iniciarlo.
4. Pulsar **Configurar** una sola vez.
5. Observar eventos correlacionados con el mismo `requestId`:

```text
wifi / in_progress
wifi / completed
claim / in_progress
claim / completed
```

El monitor del mock debe mostrar `Registro recibido`. El dispositivo conserva el
control local mientras reintenta errores transitorios con espera incremental.

## 4. Resultado esperado

Al volver a **Identificar**:

- `provisioningState` es `provisioned`;
- `wifiConfigured` es `true`;
- en `1.6.0`, `mqttConfigured` continúa `false`; desde `1.7.0` el flujo sigue
  automáticamente con credenciales MQTT según la guía de las fases 10 y 11;
- no aparecen contraseña Wi‑Fi ni token en las respuestas del ESP.

Es normal que el firmware de desarrollo anuncie después la solicitud manual de
credenciales MQTT heredada de la fase 5. No hay que procesarla para cerrar las fases
8 y 9.

## 5. Casos negativos mínimos

- Token distinto: resultado `claim / failed / invalid_provisioning_token`.
- SSID inexistente: resultado `wifi / failed / wifi_connection_failed`; se restaura
  la red anterior o permanece disponible el portal si no existía una configuración.
- URL inválida, SSID vacío, versión distinta de `1` o campos adicionales: el ESP
  responde `bootstrap / failed / invalid_contract` sin modificar NVS.
- Mock detenido: se emite `claim / retrying / network_error`; sensores y actuadores
  continúan en el ciclo local.
- Reinicio durante el claim: se recuperan URL, token temporal y solicitud pendiente,
  evitando crear claims continuamente.

## 6. Pruebas automatizadas

```powershell
node --test .\test\phase9_managed_provisioning_mock.test.mjs
platformio run
```

