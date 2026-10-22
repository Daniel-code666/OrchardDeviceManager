[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('list', 'upload', 'upload-all', 'monitor', 'erase', 'factory-reset')]
    [string]$Command = 'list',

    [string]$DeviceId,
    [string]$Serial,
    [string]$Port,
    [int]$Baud = 115200
)

$ErrorActionPreference = 'Stop'

function Get-PlatformIoPath {
    $command = Get-Command platformio -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidate = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
    if (Test-Path -LiteralPath $candidate) {
        return $candidate
    }

    throw 'No se encontro PlatformIO. Instálalo o agrega platformio al PATH.'
}

function Normalize-UsbSerial([string]$Value) {
    return ($Value -replace '[^0-9A-Za-z]', '').ToLowerInvariant()
}

function Get-HuertaPorts([string]$PlatformIo) {
    $json = (& $PlatformIo device list --json-output 2>$null) -join "`n"
    if (-not $json.Trim()) {
        return @()
    }

    $devices = @($json | ConvertFrom-Json)
    return @($devices | Where-Object { $_.hwid -match 'VID:PID=303A:' } | ForEach-Object {
        $usbSerial = ''
        if ($_.hwid -match 'SER=([^\s]+)') {
            $usbSerial = $Matches[1]
        }
        [PSCustomObject]@{
            Port        = $_.port
            UsbSerial   = $usbSerial
            Description = $_.description
            DeviceId    = $null
            Firmware    = $null
            Status      = 'detected'
        }
    })
}

function Read-DeviceInfo($Device, [int]$Speed) {
    $serialPort = [System.IO.Ports.SerialPort]::new(
        $Device.Port,
        $Speed,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One
    )
    try {
        $serialPort.DtrEnable = $false
        $serialPort.RtsEnable = $false
        $serialPort.ReadTimeout = 100
        $serialPort.Open()
        Start-Sleep -Milliseconds 350
        $serialPort.DiscardInBuffer()
        $serialPort.Write("DEVICE_INFO`n")

        $output = ''
        $timer = [System.Diagnostics.Stopwatch]::StartNew()
        while ($timer.ElapsedMilliseconds -lt 1800) {
            $output += $serialPort.ReadExisting()
            if ($output -match 'DEVICE_INFO_END') {
                break
            }
            Start-Sleep -Milliseconds 50
        }

        $match = [regex]::Match($output, '\{"type":"device_info"[^\r\n]*\}')
        if (-not $match.Success) {
            $Device.Status = 'legacy-or-no-response'
            return
        }

        $info = $match.Value | ConvertFrom-Json
        $Device.DeviceId = $info.deviceId
        $Device.Firmware = $info.firmwareVersion
        if (-not $Device.DeviceId -or -not $Device.Firmware) {
            $Device.Status = 'incomplete-device-info'
            return
        }
        $Device.Status = 'ready'
    }
    catch {
        $Device.Status = 'busy'
    }
    finally {
        if ($serialPort.IsOpen) {
            $serialPort.Close()
        }
        $serialPort.Dispose()
    }
}

function Resolve-HuertaPort($Devices) {
    $matches = @($Devices)
    if ($Port) {
        $matches = @($matches | Where-Object { $_.Port -ieq $Port })
    }
    if ($Serial) {
        $expectedSerial = Normalize-UsbSerial $Serial
        $matches = @($matches | Where-Object {
            (Normalize-UsbSerial $_.UsbSerial) -eq $expectedSerial
        })
    }
    if ($DeviceId) {
        foreach ($device in $matches) {
            if (-not $device.DeviceId) {
                Read-DeviceInfo $device $Baud
            }
        }
        $matches = @($matches | Where-Object { $_.DeviceId -eq $DeviceId })
    }

    if ($matches.Count -eq 0) {
        throw 'No se encontro un ESP que coincida con los selectores indicados.'
    }
    if ($matches.Count -gt 1) {
        throw 'Hay varios ESP conectados. Usa -DeviceId, -Serial o -Port para elegir uno.'
    }
    return $matches[0]
}

function Wait-HuertaPortBySerial([string]$PlatformIo, [string]$UsbSerial, [int]$TimeoutSeconds = 20) {
    $normalizedSerial = Normalize-UsbSerial $UsbSerial
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        $match = @(Get-HuertaPorts $PlatformIo | Where-Object {
            (Normalize-UsbSerial $_.UsbSerial) -eq $normalizedSerial
        }) | Select-Object -First 1
        if ($match) {
            return $match
        }
        Start-Sleep -Milliseconds 500
    }

    throw "El ESP con serial $UsbSerial no reaparecio por USB en $TimeoutSeconds segundos."
}

$platformIo = Get-PlatformIoPath
$devices = @(Get-HuertaPorts $platformIo)
if ($devices.Count -eq 0) {
    throw 'No se detectaron ESP32-C3 por USB.'
}

if ($Command -eq 'list') {
    foreach ($device in $devices) {
        Read-DeviceInfo $device $Baud
    }
    $devices | Select-Object Port, UsbSerial, DeviceId, Firmware, Status
    exit 0
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $projectRoot
try {
    if ($Command -eq 'upload-all') {
        $serials = @($devices | ForEach-Object { $_.UsbSerial })
        foreach ($usbSerial in $serials) {
            $currentDevices = @(Get-HuertaPorts $platformIo)
            $normalizedSerial = Normalize-UsbSerial $usbSerial
            $device = @($currentDevices | Where-Object {
                (Normalize-UsbSerial $_.UsbSerial) -eq $normalizedSerial
            }) | Select-Object -First 1
            if (-not $device) {
                throw "El ESP con serial $usbSerial ya no esta conectado."
            }

            Write-Host "Cargando firmware en $($device.Port) [$($device.UsbSerial)]..."
            & $platformIo run --target upload --upload-port $device.Port
            if ($LASTEXITCODE -ne 0) {
                throw "La carga fallo en $($device.Port)."
            }
        }
        exit 0
    }

    $selected = Resolve-HuertaPort $devices
    if ($Command -eq 'upload') {
        Write-Host "Cargando firmware en $($selected.Port) [$($selected.UsbSerial)]..."
        & $platformIo run --target upload --upload-port $selected.Port
        exit $LASTEXITCODE
    }

    if ($Command -eq 'erase') {
        Write-Host "Borrando por completo la flash de $($selected.Port) [$($selected.UsbSerial)]..."
        & $platformIo run --target erase --upload-port $selected.Port
        exit $LASTEXITCODE
    }

    if ($Command -eq 'factory-reset') {
        if (-not $selected.UsbSerial) {
            throw 'Factory-reset requiere que el adaptador USB exponga un numero de serie estable.'
        }

        $usbSerial = $selected.UsbSerial
        Write-Host "Borrando por completo la flash de $($selected.Port) [$usbSerial]..."
        & $platformIo run --target erase --upload-port $selected.Port
        if ($LASTEXITCODE -ne 0) {
            throw "El borrado fallo en $($selected.Port)."
        }

        $selected = Wait-HuertaPortBySerial $platformIo $usbSerial
        Write-Host "Instalando firmware limpio en $($selected.Port) [$usbSerial]..."
        & $platformIo run --target upload --upload-port $selected.Port
        if ($LASTEXITCODE -ne 0) {
            throw "La carga posterior al borrado fallo en $($selected.Port)."
        }

        Start-Sleep -Seconds 2
        $selected = Wait-HuertaPortBySerial $platformIo $usbSerial
        Read-DeviceInfo $selected $Baud
        $selected | Select-Object Port, UsbSerial, DeviceId, Firmware, Status
        exit 0
    }

    Write-Host "Abriendo $($selected.Port) [$($selected.UsbSerial)] a $Baud baudios..."
    & $platformIo device monitor --port $selected.Port --baud $Baud
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
