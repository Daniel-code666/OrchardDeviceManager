# Pruebas de aceptación — Fase 4

## Alcance implementado

- `DeviceProvisioningManager` mantiene `Unprovisioned`, `PendingClaim` y `Provisioned`.
- El estado de vinculación se persiste en NVS por separado de Wi-Fi y MQTT.
- MQTT solo se inicia cuando la vinculación está en `Provisioned`.
- El control local y las lecturas de sensores continúan aunque no haya conectividad.
- El contrato JSON y el mock no dependen de .NET ni entregan credenciales MQTT.
- Una respuesta se autentica con HMAC-SHA256 usando el código temporal de 12 caracteres.

## Preparación

1. Compilar y cargar el firmware.
2. Abrir el monitor serie a 115200 baudios.
3. Si ya pasó el arranque, enviar la línea `PROVISIONING_REQUEST` para volver a
   mostrar la solicitud pendiente.
4. Copiar únicamente el JSON situado entre `PROVISIONING_REQUEST_BEGIN` y
   `PROVISIONING_REQUEST_END` a un archivo local, por ejemplo `claim-request.json`.
5. No publicar ese archivo: contiene el código temporal de vinculación.

## Aprobación autenticada

```powershell
python tools/phase4_provisioning_mock.py --request-file claim-request.json --decision approved --user-id user-local --garden-id garden-local
```

Pegar la única línea JSON resultante en el monitor serie. Deben aparecer
`Resultado de vinculacion: approved`, `Estado de vinculacion: provisioned` y el
inicio normal de MQTT. Reiniciar el ESP32 y comprobar que restaura
`provisioned` sin crear otra solicitud.

## Mensaje no autenticado

```powershell
python tools/phase4_provisioning_mock.py --request-file claim-request.json --decision approved --user-id user-local --garden-id garden-local --invalid-authentication
```

El resultado esperado es `authentication_failed`; el estado debe seguir en
`pending_claim` y MQTT no debe iniciarse.

## Rechazo

```powershell
python tools/phase4_provisioning_mock.py --request-file claim-request.json --decision rejected
```

El resultado esperado es `rejected` seguido por `unprovisioned`. No debe
guardarse `UserId` ni `GardenId`.

## Caducidad

Puede probarse esperando cinco minutos sin responder o firmando una decisión
`expired` con el mock. En ambos casos el dispositivo vuelve a `unprovisioned`.
Una respuesta posterior para el `RequestId` anterior se rechaza por estado inválido.

## Reinicios y conectividad

- Reiniciar durante `pending_claim`: se restauran el mismo `RequestId` y código,
  y comienza una nueva ventana local de cinco minutos para completar la prueba.
- Reiniciar después de `provisioned`: la vinculación se conserva.
- Apagar Wi-Fi o RabbitMQ: la vinculación permanece `provisioned`; únicamente
  cambia el estado de conectividad gestionado por sus componentes existentes.

## Transiciones deliberadamente ausentes

La desvinculación y reasignación pertenecen a la Fase 6. No existe una orden
local que permita pasar de `Provisioned` a `Unprovisioned`.

