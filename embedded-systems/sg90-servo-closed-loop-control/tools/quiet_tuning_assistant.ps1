#requires -Version 5.1
<#
.SYNOPSIS
    Windows 版静音调参助手：读取串口 CSV 数据并实时提示手该往哪偏。

.DESCRIPTION
    串口格式：TIME_MS,MODE,TARGET,YAW,OUTPUT,KP,KI,KD
    无需 WSL2，直接在 Windows PowerShell 下运行，解决 WSL2 USB 串口桥接不稳定问题。

.PARAMETER Port
    串口号，例如 COM3

.PARAMETER Baud
    波特率，默认 115200

.PARAMETER Output
    同时保存的 CSV 日志路径

.PARAMETER Window
    调参建议滑动窗口样本数，默认 50

.EXAMPLE
    .\quiet_tuning_assistant.ps1 -Port COM3
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [int]$Baud = 115200,

    [string]$Output = $null,

    [int]$Window = 50
)

# 加载 System.IO.Ports
Add-Type -AssemblyName System.Windows.Forms | Out-Null

function Clamp($val, $lo, $hi) {
    return [Math]::Max($lo, [Math]::Min($hi, $val))
}

function Get-DirectionHint($error) {
    if ([Math]::Abs($error) -lt 2.0) {
        return "  ✓ 保持不动  "
    }
    if ($error -gt 0) {
        return "  <<< 逆时针转  "
    }
    return "  顺时针转 >>>  "
}

function Get-TuningAdvice($errors, $outputs, $kp, $ki, $kd) {
    if ($errors.Count -lt 20) {
        return "数据不足，继续采样..."
    }

    $err = $errors[-1]
    $errAbs = [Math]::Abs($err)
    $errSign = if ($err -ge 0) { 1 } else { -1 }
    $sameSign = ($errors | Where-Object { ($_ -ge 0) -eq ($errSign -ge 0) }).Count
    $ratio = $sameSign / $errors.Count

    $meanOut = ($outputs | Measure-Object -Average).Average
    $crossings = 0
    for ($i = 1; $i -lt $outputs.Count; $i++) {
        $prev = $outputs[$i - 1] - $meanOut
        $cur = $outputs[$i] - $meanOut
        if (($prev * $cur) -lt 0) { $crossings++ }
    }

    if ($errAbs -gt 20.0 -and $ratio -gt 0.85) {
        if ($kp -lt 8.0) { return "误差长期偏大 → 可谨慎增大 Kp" }
        return "误差仍大但 Kp 已高 → 检查机械/安装或降低 Kp"
    }

    if ($crossings -gt ($outputs.Count * 0.25) -and $errAbs -lt 10.0) {
        if ($kd -lt 0.5) { return "输出震荡 → 增大 Kd 或减小 Kp" }
        return "输出仍震荡 → 继续减小 Kp"
    }

    if ($errAbs -lt 5.0 -and $ratio -gt 0.75) {
        if ($ki -lt 0.05) { return "接近目标但有小稳态误差 → 可微调 Ki" }
        return "稳态误差仍存在 → 检查积分分离阈值或 Ki 是否过大"
    }

    if ($errAbs -lt 2.0) { return "当前跟踪良好，保持参数" }
    return "继续观察..."
}

# 默认日志名
if (-not $Output) {
    $Output = "quiet_tune_$(Get-Date -Format 'yyyyMMdd_HHmmss').csv"
}
$Output = Resolve-Path -Path $Output -ErrorAction SilentlyContinue
if (-not $Output) {
    $Output = Join-Path (Get-Location) "quiet_tune_$(Get-Date -Format 'yyyyMMdd_HHmmss').csv"
}

Write-Host "[INFO] 打开串口 $Port @ $Baud ..."
try {
    $serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, "None", 8, "One"
    $serial.ReadTimeout = 1000
    $serial.Open()
} catch {
    Write-Host "[ERROR] 无法打开串口 ${Port}: $_" -ForegroundColor Red
    exit 1
}

Write-Host "[INFO] 开始静音调参辅助，日志保存到 $Output"
Write-Host "[INFO] 按 Ctrl+C 停止`n"

# 创建 CSV 日志
"TIME_MS,MODE,TARGET,YAW,OUTPUT,KP,KI,KD" | Out-File -FilePath $Output -Encoding utf8

$errors = New-Object System.Collections.Generic.Queue[float]
$outputs = New-Object System.Collections.Generic.Queue[float]
$lastUiUpdate = [DateTime]::MinValue

try {
    while ($true) {
        $line = $null
        try {
            $line = $serial.ReadLine()
        } catch [System.TimeoutException] {
            continue
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

        $errors.Enqueue($error)
        $outputs.Enqueue($output)
        while ($errors.Count -gt $Window) { $errors.Dequeue() | Out-Null }
        while ($outputs.Count -gt $Window) { $outputs.Dequeue() | Out-Null }

        # 写入 CSV
        "$tick,$mode,$target,$yaw,$output,$kp,$ki,$kd" | Out-File -FilePath $Output -Encoding utf8 -Append

        # 每 200 ms 刷新一次 UI
        $now = Get-Date
        if (($now - $lastUiUpdate).TotalSeconds -ge 0.2) {
            $lastUiUpdate = $now
            $hint = Get-DirectionHint $error
            $advice = Get-TuningAdvice $errors $outputs $kp $ki $kd
            $modeStr = if ($mode -ne 0) { "闭环" } else { "静音" }

            $ui = "[{0}] TGT={1,7:F2} YAW={2,7:F2} ERR={3,7:F2} OUT={4,7:F1} |{5}| {6}" -f `
                $modeStr, $target, $yaw, $error, $output, $hint, $advice

            Write-Host "`r$ui" -NoNewline
        }
    }
} finally {
    Write-Host "`n[INFO] 已停止"
    if ($serial -and $serial.IsOpen) { $serial.Close() }
}
