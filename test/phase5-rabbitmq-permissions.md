# Permisos RabbitMQ por dispositivo — Fase 5

El usuario MQTT individual conserva permisos de recurso para operar el plugin,
pero sus permisos de topic sobre `amq.topic` deben limitarse a:

- escritura: `gardens.<DeviceId>.telemetry` y `gardens.<DeviceId>.actions-response`;
- lectura: `gardens.<DeviceId>.actions`.

Ejecutar `phase5_rabbitmq_permissions.py` con host, puerto, usuario y contraseña
en variables de entorno. La prueba confirma los topics permitidos y verifica que
RabbitMQ desconecta la identidad al intentar leer o escribir topics ajenos.

