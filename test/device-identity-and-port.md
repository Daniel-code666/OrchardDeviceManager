# Identidad física y selección automática del puerto

## Identidad

Cada ESP32-C3 construye su identidad estable a partir de la MAC de estación
almacenada en eFuse:

- `DeviceId`: `huerta-c3-<mac completa sin separadores>`.
- MQTT client ID: `mqtt-<DeviceId>`.
- SoftAP visible: `Huerta-<últimos seis caracteres>`.

La MAC identifica al hardware, pero no es una credencial. La clave local, la
contraseña del portal y las credenciales MQTT continúan siendo individuales.

Al arrancar una versión anterior:

- una identidad ya vinculada se conserva para no cortar su propiedad ni sus
  credenciales;
- una identidad no vinculada o con claim pendiente migra a la identidad física,
  elimina solicitudes ligadas al identificador anterior y genera un claim nuevo.

## Comando serie de descubrimiento

Con el firmware iniciado, enviar:

```text
DEVICE_INFO
```

La respuesta contiene únicamente identificadores no secretos y la versión del
firmware entre `DEVICE_INFO_BEGIN` y `DEVICE_INFO_END`.

## Herramienta local

No se fijan `upload_port` ni `monitor_port` en `platformio.ini`.

```powershell
.\tools\huerta-device.ps1 list
.\tools\huerta-device.ps1 upload
.\tools\huerta-device.ps1 monitor
.\tools\huerta-device.ps1 erase
.\tools\huerta-device.ps1 factory-reset
```

Con varios equipos conectados puede seleccionarse uno por identidad o serial USB:

```powershell
.\tools\huerta-device.ps1 monitor -DeviceId huerta-c3-a0f262a61a68
.\tools\huerta-device.ps1 upload -Serial A0:F2:62:A6:1A:68
.\tools\huerta-device.ps1 upload-all
```

La opción sin selector solo se acepta cuando hay un único ESP conectado.

`erase` elimina toda la flash, incluido NVS, pero no instala firmware. El comando
`factory-reset` ejecuta el flujo completo: selecciona el equipo, conserva su
serial USB para seguirlo si cambia de COM, borra la flash, carga el firmware y
vuelve a consultar `DEVICE_INFO`.

Ejemplo para el segundo ESP:

```powershell
.\tools\huerta-device.ps1 factory-reset -Serial A0:F2:62:A6:1A:68
```
