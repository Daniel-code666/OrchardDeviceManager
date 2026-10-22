# Pruebas de aceptación — Fase 3

## Topics esperados

Para `DeviceId = huerta-esp32-01`:

- `gardens/huerta-esp32-01/telemetry`
- `gardens/huerta-esp32-01/actions`
- `gardens/huerta-esp32-01/actions-response`

RabbitMQ traduce el topic MQTT de telemetría al routing key AMQP `gardens.huerta-esp32-01.telemetry`. La cola durable compartida `Telemetry` utiliza el binding `gardens.*.telemetry`.

## Simulación de dos dispositivos

Usar dos configuraciones válidas:

| Configuración | DeviceId | MQTT Client ID |
|---|---|---|
| A | `huerta-simulada-a` | `mqtt-huerta-simulada-a` |
| B | `huerta-simulada-b` | `mqtt-huerta-simulada-b` |

Comprobaciones:

1. Los seis topics generados son distintos entre dispositivos.
2. A se suscribe únicamente a `gardens/huerta-simulada-a/actions`.
3. B se suscribe únicamente a `gardens/huerta-simulada-b/actions`.
4. Publicar una orden en el topic de B no debe producir ejecución ni respuesta de A.
5. La respuesta de B debe aparecer solamente en `gardens/huerta-simulada-b/actions-response`.
6. La telemetría de ambos dispositivos debe llegar a la cola durable `Telemetry`.
7. Un `DeviceId` que contenga `/`, `+`, `#`, espacios o esté vacío debe ser rechazado antes de iniciar MQTT.

No reutilizar el mismo MQTT Client ID para los dos dispositivos: el broker desconectaría la sesión anterior.
El firmware conecta con `cleanSession=true`; no solicita conservar comandos publicados mientras permanece desconectado.
