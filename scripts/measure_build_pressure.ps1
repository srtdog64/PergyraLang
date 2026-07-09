param(
    [string]$Label = "dev-compiler",
    [string]$Command = "mingw32-make",
    [string[]]$Arguments = @("dev-compiler"),
    [int]$LimitMB = 3072,
    [int]$IntervalMs = 500,
    [int]$TimeoutSec = 0,
    [string]$OutDir = ".tmp/build-pressure"
)

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$safeLabel = ($Label -replace '[^A-Za-z0-9_.-]', '-')
$samplePath = Join-Path $OutDir "$safeLabel.samples.csv"
$stdoutPath = Join-Path $OutDir "$safeLabel.stdout.log"
$stderrPath = Join-Path $OutDir "$safeLabel.stderr.log"

"elapsed_ms,proc_count,working_set_mb,private_mb,max_proc,top_private_mb" |
    Set-Content -Encoding ASCII -Path $samplePath

if (Test-Path $stdoutPath) {
    Remove-Item -LiteralPath $stdoutPath -Force
}
if (Test-Path $stderrPath) {
    Remove-Item -LiteralPath $stderrPath -Force
}

$startInfo = @{
    FilePath = $Command
    ArgumentList = $Arguments
    PassThru = $true
    NoNewWindow = $true
    RedirectStandardOutput = $stdoutPath
    RedirectStandardError = $stderrPath
}

$process = Start-Process @startInfo
$started = Get-Date
$peakWorkingSet = 0.0
$peakPrivate = 0.0
$peakProcessCount = 0
$peakName = ""
$peakTopPrivate = 0.0
$timedOut = $false

function Get-ProcessTreeIds {
    param([int]$RootPid)

    $all = Get-CimInstance Win32_Process |
        Select-Object ProcessId, ParentProcessId
    $byParent = @{}
    foreach ($p in $all) {
        $parent = [int]$p.ParentProcessId
        if (-not $byParent.ContainsKey($parent)) {
            $byParent[$parent] = New-Object System.Collections.Generic.List[int]
        }
        $byParent[$parent].Add([int]$p.ProcessId)
    }

    $result = New-Object System.Collections.Generic.List[int]
    $queue = New-Object System.Collections.Generic.Queue[int]
    $queue.Enqueue($RootPid)
    while ($queue.Count -gt 0) {
        $currentPid = $queue.Dequeue()
        $result.Add($currentPid)
        if ($byParent.ContainsKey($currentPid)) {
            foreach ($child in $byParent[$currentPid]) {
                $queue.Enqueue($child)
            }
        }
    }
    return $result
}

while (-not $process.HasExited) {
    $ids = Get-ProcessTreeIds -RootPid $process.Id
    $procs = @()
    foreach ($id in $ids) {
        $p = Get-Process -Id $id -ErrorAction SilentlyContinue
        if ($null -ne $p) {
            $procs += $p
        }
    }

    $workingSet = 0.0
    $private = 0.0
    $topName = ""
    $topPrivate = 0.0
    foreach ($p in $procs) {
        $workingSet += $p.WorkingSet64 / 1MB
        $private += $p.PrivateMemorySize64 / 1MB
        $pPrivate = $p.PrivateMemorySize64 / 1MB
        if ($pPrivate -gt $topPrivate) {
            $topPrivate = $pPrivate
            $topName = "$($p.ProcessName).exe"
        }
    }

    $elapsed = [int]((Get-Date) - $started).TotalMilliseconds
    $line = "{0},{1},{2:N1},{3:N1},{4},{5:N1}" -f `
        $elapsed, $procs.Count, $workingSet, $private, $topName, $topPrivate
    Add-Content -Encoding ASCII -Path $samplePath -Value $line

    if ($workingSet -gt $peakWorkingSet) {
        $peakWorkingSet = $workingSet
    }
    if ($private -gt $peakPrivate) {
        $peakPrivate = $private
        $peakProcessCount = $procs.Count
        $peakName = $topName
        $peakTopPrivate = $topPrivate
    }

    if ($TimeoutSec -gt 0 -and ((Get-Date) - $started).TotalSeconds -ge $TimeoutSec) {
        $timedOut = $true
        foreach ($id in ($ids | Sort-Object -Descending)) {
            Stop-Process -Id $id -Force -ErrorAction SilentlyContinue
        }
        break
    }

    Start-Sleep -Milliseconds $IntervalMs
    $process.Refresh()
}

$process.WaitForExit()
$process.Refresh()
$exitCode = $process.ExitCode
if ($null -eq $exitCode) {
    $exitCode = 0
}
if ($timedOut) {
    $exitCode = 124
}

Write-Output ("[build-pressure] label={0} exit={1} peak_working_set_mb={2:N1} peak_private_mb={3:N1} peak_processes={4} top_private={5}:{6:N1} samples={7}" -f `
    $Label, $exitCode, $peakWorkingSet, $peakPrivate, $peakProcessCount, $peakName, $peakTopPrivate, $samplePath)

if ($timedOut) {
    [Console]::Error.WriteLine(("[build-pressure] timed out after {0}s" -f $TimeoutSec))
}

if ($peakPrivate -gt $LimitMB -or $peakWorkingSet -gt $LimitMB) {
    [Console]::Error.WriteLine(("[build-pressure] peak exceeded limit {0} MB; this is a compiler/build memory bug until proven otherwise" -f $LimitMB))
    exit 88
}

exit $exitCode
