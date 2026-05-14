param(
    [string]$PgyBin = ".\bin\pgy.exe",
    [string]$Cc = "gcc",
    [int]$Runs = 7,
    [int]$Warmup = 2,
    [double]$MaxRatio = 3.0
)

$ErrorActionPreference = "Stop"

if ($Runs -le $Warmup) {
    throw "Runs must be greater than Warmup"
}

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$pgy = (Resolve-Path (Join-Path $root $PgyBin)).Path
$work = Join-Path ([System.IO.Path]::GetTempPath()) ("pgy_c_baseline_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force $work | Out-Null

function Invoke-CheckedNative {
    param(
        [string]$Exe,
        [string[]]$ArgList
    )

    function Quote-NativeArg([string]$Arg) {
        if ($Arg -notmatch '[\s"]') {
            return $Arg
        }
        return '"' + ($Arg -replace '"', '\"') + '"'
    }

    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $Exe
    $psi.Arguments = ($ArgList | ForEach-Object { Quote-NativeArg $_ }) -join " "
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false

    $proc = [System.Diagnostics.Process]::Start($psi)
    $stdout = $proc.StandardOutput.ReadToEnd()
    $stderr = $proc.StandardError.ReadToEnd()
    $proc.WaitForExit()
    if ($proc.ExitCode -ne 0) {
        throw "$Exe $($psi.Arguments) failed with exit code $($proc.ExitCode): $stdout $stderr"
    }
}

try {
    $pgySource = Join-Path $root "tests\perf\c_baseline_arith_loop.pgy"
    $cSource = Join-Path $root "tests\perf\c_baseline_arith_loop.c"
    $pgyExe = Join-Path $work "arith_loop_pgy.exe"
    $cExe = Join-Path $work "arith_loop_c.exe"
    $pgyC = Join-Path $work "arith_loop_pgy.c"

    Invoke-CheckedNative -Exe $pgy -ArgList @($pgySource, "--backend=c", "--emit-c", "-o", $pgyC)
    $generated = Get-Content $pgyC -Raw
    if ($generated.Contains("pgy_checked_mod_i32_export")) {
        throw "constant nonzero modulo regressed to checked helper"
    }
    if ($generated.Contains("pgy_checked_div_i32_export")) {
        throw "constant nonzero division regressed to checked helper"
    }

    Invoke-CheckedNative -Exe $pgy -ArgList @($pgySource, "--backend=c", "--opt=release", "-o", $pgyExe)
    Invoke-CheckedNative -Exe $Cc -ArgList @("-O3", "-std=c11", $cSource, "-o", $cExe)

    $pgyOut = (& $pgyExe).Trim()
    $cOut = (& $cExe).Trim()
    if ($pgyOut -ne $cOut) {
        throw "output mismatch pgy=$pgyOut c=$cOut"
    }

    function Measure-AvgMs([string]$Exe) {
        $samples = @()
        for ($i = 0; $i -lt $Runs; $i++) {
            $sw = [System.Diagnostics.Stopwatch]::StartNew()
            & $Exe | Out-Null
            $sw.Stop()
            if ($i -ge $Warmup) {
                $samples += $sw.Elapsed.TotalMilliseconds
            }
        }
        return ($samples | Measure-Object -Average).Average
    }

    $pgyMs = Measure-AvgMs $pgyExe
    $cMs = Measure-AvgMs $cExe
    $ratio = $pgyMs / $cMs

    Write-Output "[perf-c-baseline] fixture=arith_loop output=$pgyOut"
    Write-Output ("[perf-c-baseline] pgy_generated_c_avg_ms={0:N3}" -f $pgyMs)
    Write-Output ("[perf-c-baseline] native_c_avg_ms={0:N3}" -f $cMs)
    Write-Output ("[perf-c-baseline] pgy_over_c_ratio={0:N3}" -f $ratio)
    Write-Output ("[perf-c-baseline] verdict={0}" -f ($(if ($ratio -lt 1.0) { "pgy-generated-c-faster" } else { "native-c-faster" })))

    if ($ratio -gt $MaxRatio) {
        throw ("ratio {0:N3} exceeds max {1:N3}" -f $ratio, $MaxRatio)
    }
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
