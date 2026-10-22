# Pruebas de aceptación — Fase 6

## Recuperación implementada

- La pérdida de Wi-Fi abre el portal local después de 30 segundos y no cambia
  la vinculación ni el `DeviceId`.
- Una credencial MQTT candidata solo se confirma tras una conexión estable; un
  fallo o reinicio conserva la configuración activa anterior.
- Cada escritura de configuración mantiene una copia completa última conocida
  válida; si el bloque principal queda incompleto, el arranque la restaura.
- La revocación y la desvinculación se reciben como comandos HMAC-SHA256
  autenticados con la clave individual del dispositivo.
- La versión monotónica y el `RequestId` persistidos impiden repetir comandos.
- La revocación se guarda antes de desconectar MQTT y sobrevive reinicios.
- La desvinculación usa una marca transaccional. Si se reinicia durante el
  proceso, el siguiente arranque termina de borrar el material criptográfico.
- Un borrado local de Wi-Fi no cambia la propiedad del dispositivo.

## Generar una orden local

Obtener la clave mostrada entre `MQTT_CREDENTIALS_LOCAL_KEY_BEGIN` y
`MQTT_CREDENTIALS_LOCAL_KEY_END`. Este valor solo se expone porque el mock local
de desarrollo está habilitado.

```powershell
python tools/phase6_recovery_mock.py --device-id huerta-esp32-01 --local-key CLAVE_LOCAL --action revoke_mqtt --command-version 1
```

Pegar la única línea JSON en el monitor serie. Se espera
`Resultado de recuperacion: applied`, desconexión inmediata de MQTT y una nueva
solicitud cifrada de credenciales. Cuando una versión nueva se conecta de forma
estable, MQTT vuelve a quedar habilitado.

Para terminar la vinculación autorizadamente:

```powershell
python tools/phase6_recovery_mock.py --device-id huerta-esp32-01 --local-key CLAVE_LOCAL --action unlink --command-version 2
```

Se espera que MQTT se detenga, se elimine la clave de aprovisionamiento anterior
y aparezca una nueva solicitud de vinculación. El `DeviceId` y la configuración
Wi-Fi se conservan. Después de aprobar el nuevo vínculo se genera una clave AES
nueva y se solicitan credenciales MQTT nuevas.

## Casos negativos

- `--invalid-authentication`: devuelve `authentication_failed` y no cambia NVS.
- Repetir versión o `RequestId`: devuelve `replay_rejected`.
- Orden para otro `DeviceId`: devuelve `invalid_contract`.
- Orden estando sin vincular: devuelve `invalid_state`.
- Cortar alimentación tras aceptar `unlink`: el arranque termina la limpieza y
  no conecta con las credenciales antiguas.

## Pruebas automatizadas

```powershell
python test/test_phase6_recovery_mock.py
```

