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
$summaryPath = Join-Path $OutDir "$safeLabel.summary.json"
$stdoutPath = Join-Path $OutDir "$safeLabel.stdout.log"
$stderrPath = Join-Path $OutDir "$safeLabel.stderr.log"

"elapsed_ms,phase,proc_count,compile_proc_count,link_proc_count,working_set_mb,private_mb,max_proc,top_private_mb" |
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
$phaseStats = @{
    orchestrate = @{ Samples = 0; PeakWorkingSet = 0.0; PeakPrivate = 0.0 }
    compile = @{ Samples = 0; PeakWorkingSet = 0.0; PeakPrivate = 0.0 }
    link = @{ Samples = 0; PeakWorkingSet = 0.0; PeakPrivate = 0.0 }
}

function Get-ProcessTreeRows {
    param([int]$RootPid)

    $all = Get-CimInstance Win32_Process |
        Select-Object ProcessId, ParentProcessId, Name, CommandLine
    $byParent = @{}
    $byId = @{}
    foreach ($p in $all) {
        $byId[[int]$p.ProcessId] = $p
        $parent = [int]$p.ParentProcessId
        if (-not $byParent.ContainsKey($parent)) {
            $byParent[$parent] = New-Object System.Collections.Generic.List[int]
        }
        $byParent[$parent].Add([int]$p.ProcessId)
    }

    $result = New-Object System.Collections.Generic.List[object]
    $queue = New-Object System.Collections.Generic.Queue[int]
    $queue.Enqueue($RootPid)
    while ($queue.Count -gt 0) {
        $currentPid = $queue.Dequeue()
        if ($byId.ContainsKey($currentPid)) {
            $result.Add($byId[$currentPid])
        }
        if ($byParent.ContainsKey($currentPid)) {
            foreach ($child in $byParent[$currentPid]) {
                $queue.Enqueue($child)
            }
        }
    }
    return $result
}

while (-not $process.HasExited) {
    $rows = Get-ProcessTreeRows -RootPid $process.Id
    $ids = @($rows | ForEach-Object { [int]$_.ProcessId })
    $procs = @()
    $compileProcCount = 0
    $linkProcCount = 0
    foreach ($row in $rows) {
        $p = Get-Process -Id ([int]$row.ProcessId) -ErrorAction SilentlyContinue
        if ($null -ne $p) {
            $procs += $p
        }
        $commandLine = [string]$row.CommandLine
        $processName = [string]$row.Name
        if ($commandLine -match '(^|\s)-c(\s|$)') {
            $compileProcCount++
        }
        elseif ($processName -match '^(cc|gcc|g\+\+|clang|clang\+\+)(\.exe)?$' `
            -and $commandLine -match '(^|\s)-o(\s|$)') {
            $linkProcCount++
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

    $phase = if ($linkProcCount -gt 0) {
        "link"
    }
    elseif ($compileProcCount -gt 0) {
        "compile"
    }
    else {
        "orchestrate"
    }
    $phaseStat = $phaseStats[$phase]
    $phaseStat.Samples++
    if ($workingSet -gt $phaseStat.PeakWorkingSet) {
        $phaseStat.PeakWorkingSet = $workingSet
    }
    if ($private -gt $phaseStat.PeakPrivate) {
        $phaseStat.PeakPrivate = $private
    }

    $elapsed = [int]((Get-Date) - $started).TotalMilliseconds
    $line = "{0},{1},{2},{3},{4},{5:N1},{6:N1},{7},{8:N1}" -f `
        $elapsed, $phase, $procs.Count, $compileProcCount, $linkProcCount, `
        $workingSet, $private, $topName, $topPrivate
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

$elapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
$summary = [ordered]@{
    schema = "pgy.build-pressure.v2"
    label = $Label
    exit_code = $exitCode
    elapsed_ms = $elapsedMs
    interval_ms = $IntervalMs
    peak_working_set_mb = [math]::Round($peakWorkingSet, 1)
    peak_private_mb = [math]::Round($peakPrivate, 1)
    peak_processes = $peakProcessCount
    top_private_process = $peakName
    top_private_mb = [math]::Round($peakTopPrivate, 1)
    phases = [ordered]@{
        orchestrate = $phaseStats.orchestrate
        compile = $phaseStats.compile
        link = $phaseStats.link
    }
    samples = $samplePath
}
$summary | ConvertTo-Json -Depth 4 | Set-Content -Encoding ASCII -Path $summaryPath

Write-Output ("[build-pressure] label={0} exit={1} elapsed_ms={2} peak_working_set_mb={3:N1} peak_private_mb={4:N1} peak_processes={5} top_private={6}:{7:N1} samples={8} summary={9}" -f `
    $Label, $exitCode, $elapsedMs, $peakWorkingSet, $peakPrivate, `
    $peakProcessCount, $peakName, $peakTopPrivate, $samplePath, $summaryPath)

if ($timedOut) {
    [Console]::Error.WriteLine(("[build-pressure] timed out after {0}s" -f $TimeoutSec))
}

if ($peakPrivate -gt $LimitMB -or $peakWorkingSet -gt $LimitMB) {
    [Console]::Error.WriteLine(("[build-pressure] peak exceeded limit {0} MB; this is a compiler/build memory bug until proven otherwise" -f $LimitMB))
    exit 88
}

exit $exitCode
