#requires -Version 5.1
<#
.SYNOPSIS
    Windows quiet tuning assistant for SG90 closed-loop project.

.DESCRIPTION
    Reads serial CSV: TIME_MS,MODE,TARGET,YAW,OUTPUT,KP,KI,KD
    Tells you which way to turn the MPU6050 by hand.

.PARAMETER Port
    Serial port, e.g. COM3

.PARAMETER Baud
    Baud rate, default 115200

.PARAMETER Output
    CSV log file path

.PARAMETER Window
    Sliding window size for advice, default 50

.EXAMPLE
    .\quiet_tuning_assistant_ascii.ps1 -Port COM3
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [int]$Baud = 115200,

    [string]$Output = $null,

    [int]$Window = 50
)

function Get-DirectionHint($error) {
    if ([Math]::Abs($error) -lt 2.0) {
        return "  OK HOLD     "
    }
    if ($error -gt 0) {
        return "  <<< CCW     "
    }
    return "  CW >>>      "
}

function Get-TuningAdvice($errors, $outputs, $kp, $ki, $kd) {
    if ($errors.Count -lt 20) {
        return "collecting data..."
    }

    $err = [float]$errors[$errors.Count - 1]
    $errAbs = [Math]::Abs($err)
    $errSign = 1
    if ($err -lt 0) { $errSign = -1 }

    $sameSign = 0
    foreach ($e in $errors) {
        $es = 1
        if ($e -lt 0) { $es = -1 }
        if ($es -eq $errSign) { $sameSign++ }
    }
    $ratio = $sameSign / $errors.Count

    $sumOut = 0.0
    foreach ($o in $outputs) { $sumOut += $o }
    $meanOut = $sumOut / $outputs.Count

    $crossings = 0
    for ($i = 1; $i -lt $outputs.Count; $i++) {
        $prev = $outputs[$i - 1] - $meanOut
        $cur = $outputs[$i] - $meanOut
        if (($prev * $cur) -lt 0) { $crossings++ }
    }

    if ($errAbs -gt 20.0 -and $ratio -gt 0.85) {
        if ($kp -lt 8.0) { return "large steady error -> try increase Kp" }
        return "error still large but Kp high -> check hardware"
    }

    if ($crossings -gt ($outputs.Count * 0.25) -and $errAbs -lt 10.0) {
        if ($kd -lt 0.5) { return "output oscillating -> increase Kd or decrease Kp" }
        return "still oscillating -> decrease Kp"
    }

    if ($errAbs -lt 5.0 -and $ratio -gt 0.75) {
        if ($ki -lt 0.05) { return "small steady error -> try small Ki" }
        return "steady error remains -> check Ki / integral"
    }

    if ($errAbs -lt 2.0) { return "tracking OK, keep params" }
    return "keep observing..."
}

if (-not $Output) {
    $Output = "quiet_tune_$(Get-Date -Format 'yyyyMMdd_HHmmss').csv"
}
$resolved = Resolve-Path -Path $Output -ErrorAction SilentlyContinue
if (-not $resolved) {
    $Output = Join-Path (Get-Location) "quiet_tune_$(Get-Date -Format 'yyyyMMdd_HHmmss').csv"
}

Write-Host "[INFO] Opening $Port @ $Baud ..."
try {
    $serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, "None", 8, "One"
    $serial.ReadTimeout = 1000
    $serial.Open()
} catch {
    Write-Host "[ERROR] Cannot open ${Port}: $_" -ForegroundColor Red
    exit 1
}

Write-Host "[INFO] Logging to $Output"
Write-Host "[INFO] Press Ctrl+C to stop`n"

"TIME_MS,MODE,TARGET,YAW,OUTPUT,KP,KI,KD" | Out-File -FilePath $Output -Encoding ascii

$errors = New-Object System.Collections.ArrayList
$outputs = New-Object System.Collections.ArrayList
$lastUiUpdate = [DateTime]::MinValue

try {
    while ($true) {
        $line = $null
        try {
            $line = $serial.ReadLine()
        } catch {
            continue
        }

        if (-not $line) { continue }
        $line = $line.Trim()
        $parts = $line -split ","
        if ($parts.Count -ne 8) { continue }

        try {
            [uint32]$tick = $parts[0]
            [int]$mode = $parts[1]
            [float]$target = $parts[2]
            [float]$yaw = $parts[3]
            [float]$output = $parts[4]
            [float]$kp = $parts[5]
            [float]$ki = $parts[6]
            [float]$kd = $parts[7]
        } catch {
            continue
        }

        $error = $target - $yaw
        [void]$errors.Add($error)
        [void]$outputs.Add($output)
        while ($errors.Count -gt $Window) { [void]$errors.RemoveAt(0) }
        while ($outputs.Count -gt $Window) { [void]$outputs.RemoveAt(0) }

        "$tick,$mode,$target,$yaw,$output,$kp,$ki,$kd" | Out-File -FilePath $Output -Encoding ascii -Append

        $now = Get-Date
        if (($now - $lastUiUpdate).TotalSeconds -ge 0.2) {
            $lastUiUpdate = $now
            $hint = Get-DirectionHint $error
            $advice = Get-TuningAdvice $errors $outputs $kp $ki $kd
            $modeStr = if ($mode -ne 0) { "CLOSED" } else { "MUTE  " }

            $ui = "[{0}] TGT={1,7:N2} YAW={2,7:N2} ERR={3,7:N2} OUT={4,7:N1} |{5}| {6}" -f `
                $modeStr, $target, $yaw, $error, $output, $hint, $advice

            Write-Host "`r$ui" -NoNewline
        }
    }
} finally {
    Write-Host "`n[INFO] Stopped"
    if ($serial -and $serial.IsOpen) { $serial.Close() }
}
