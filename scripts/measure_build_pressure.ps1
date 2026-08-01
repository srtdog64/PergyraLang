param(
    [string]$Label = "dev-compiler",
    [string]$Command = "mingw32-make",
    [string[]]$Arguments = @("dev-compiler"),
    [int]$LimitMB = 3072,
    [int]$AttentionPercent = 80,
    [int]$IntervalMs = 500,
    [int]$TimeoutSec = 0,
    [int]$OutputDrainTimeoutMs = 5000,
    [switch]$StopOnLimit,
    [switch]$RootProcessTreeOnly,
    [string]$OutDir = ".tmp/build-pressure"
)

$ErrorActionPreference = "Stop"
$invariantCulture = [Globalization.CultureInfo]::InvariantCulture

if ($AttentionPercent -lt 1 -or $AttentionPercent -gt 99) {
    throw "AttentionPercent must be between 1 and 99"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$safeLabel = ($Label -replace '[^A-Za-z0-9_.-]', '-')
$samplePath = Join-Path $OutDir "$safeLabel.samples.csv"
$summaryPath = Join-Path $OutDir "$safeLabel.summary.json"
$stdoutPath = Join-Path $OutDir "$safeLabel.stdout.log"
$stderrPath = Join-Path $OutDir "$safeLabel.stderr.log"
$stagePath = Join-Path $OutDir "$safeLabel.stages.csv"

"elapsed_ms,phase,proc_count,compile_proc_count,link_proc_count,working_set_mb,private_mb,max_proc,top_private_mb" |
    Set-Content -Encoding ASCII -Path $samplePath
"observed_elapsed_ms,stream,stage" |
    Set-Content -Encoding ASCII -Path $stagePath

if (Test-Path $stdoutPath) {
    Remove-Item -LiteralPath $stdoutPath -Force
}
if (Test-Path $stderrPath) {
    Remove-Item -LiteralPath $stderrPath -Force
}

function ConvertTo-NativeArgument {
    param([string]$Value)

    if ($Value.Length -gt 0 -and $Value -notmatch '[\s"]') {
        return $Value
    }
    $builder = New-Object System.Text.StringBuilder
    [void]$builder.Append('"')
    $slashes = 0
    foreach ($ch in $Value.ToCharArray()) {
        if ($ch -eq '\') {
            $slashes++
        }
        elseif ($ch -eq '"') {
            [void]$builder.Append(('\' * (($slashes * 2) + 1)))
            [void]$builder.Append('"')
            $slashes = 0
        }
        else {
            [void]$builder.Append(('\' * $slashes))
            [void]$builder.Append($ch)
            $slashes = 0
        }
    }
    [void]$builder.Append(('\' * ($slashes * 2)))
    [void]$builder.Append('"')
    return $builder.ToString()
}

if (-not ("BuildPressureOutputCapture" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;

public sealed class BuildPressureOutputCapture : IDisposable
{
    private readonly StringBuilder stdoutText = new StringBuilder();
    private readonly StringBuilder stderrText = new StringBuilder();
    private readonly StringBuilder stdoutStageLine = new StringBuilder();
    private readonly StringBuilder stderrStageLine = new StringBuilder();
    private readonly Stopwatch clock = new Stopwatch();
    private readonly StreamWriter stageWriter;
    private StreamReader stdoutReader;
    private StreamReader stderrReader;
    private Task stdoutTask;
    private Task stderrTask;
    private volatile bool stdoutEof;
    private volatile bool stderrEof;
    private string lastObservedStage = "";
    private int observedStageCount;

    public BuildPressureOutputCapture(string stagePath)
    {
        stageWriter = new StreamWriter(
            stagePath, true, new UTF8Encoding(false));
        stageWriter.AutoFlush = true;
    }

    public void StartClock()
    {
        clock.Restart();
    }

    public void Start(StreamReader output, StreamReader error)
    {
        stdoutReader = output;
        stderrReader = error;
        stdoutTask = PumpAsync(
            stdoutReader, stdoutText, stdoutStageLine, "stdout", true);
        stderrTask = PumpAsync(
            stderrReader, stderrText, stderrStageLine, "stderr", false);
    }

    private async Task PumpAsync(
        StreamReader reader,
        StringBuilder captured,
        StringBuilder stageLine,
        string stream,
        bool isStdout)
    {
        char[] buffer = new char[4096];
        try
        {
            while (true)
            {
                int count = await reader.ReadAsync(
                    buffer, 0, buffer.Length).ConfigureAwait(false);
                if (count == 0)
                {
                    RecordTrailingStage(stream, stageLine);
                    if (isStdout) { stdoutEof = true; }
                    else { stderrEof = true; }
                    return;
                }
                lock (captured)
                {
                    captured.Append(buffer, 0, count);
                }
                CaptureStageLines(stream, stageLine, buffer, count);
            }
        }
        catch (ObjectDisposedException)
        {
        }
        catch (IOException)
        {
        }
    }

    private void CaptureStageLines(
        string stream,
        StringBuilder pending,
        char[] buffer,
        int count)
    {
        for (int i = 0; i < count; i++)
        {
            char current = buffer[i];
            if (current == '\n')
            {
                RecordTrailingStage(stream, pending);
                pending.Clear();
            }
            else
            {
                pending.Append(current);
            }
        }
    }

    private void RecordTrailingStage(string stream, StringBuilder pending)
    {
        if (pending.Length == 0)
        {
            return;
        }
        string line = pending.ToString();
        if (line.EndsWith("\r", StringComparison.Ordinal))
        {
            line = line.Substring(0, line.Length - 1);
        }
        if (!line.StartsWith(
                "[driver-pressure-stage]", StringComparison.Ordinal) &&
            !line.StartsWith(
                "[semantic-body-type-stage]", StringComparison.Ordinal) &&
            !line.StartsWith(
                "[semantic-initializer-stage]", StringComparison.Ordinal) &&
            !line.StartsWith(
                "[codegen-view-stage]", StringComparison.Ordinal) &&
            !line.StartsWith(
                "[codegen-pressure-stage]", StringComparison.Ordinal))
        {
            return;
        }
        string escapedStage = line.Replace("\"", "\"\"");
        lock (stageWriter)
        {
            stageWriter.WriteLine(
                "{0},{1},\"{2}\"",
                clock.ElapsedMilliseconds,
                stream,
                escapedStage);
            lastObservedStage = line;
            observedStageCount++;
        }
    }

    public string LastObservedStage()
    {
        lock (stageWriter) { return lastObservedStage; }
    }

    public int ObservedStageCount()
    {
        lock (stageWriter) { return observedStageCount; }
    }

    public string StandardOutputText()
    {
        lock (stdoutText) { return stdoutText.ToString(); }
    }

    public string StandardErrorText()
    {
        lock (stderrText) { return stderrText.ToString(); }
    }

    public bool WaitForCompletion(int timeoutMs)
    {
        if (stdoutTask == null || stderrTask == null)
        {
            return false;
        }
        try
        {
            return Task.WaitAll(
                new Task[] { stdoutTask, stderrTask }, timeoutMs) &&
                stdoutEof && stderrEof;
        }
        catch (AggregateException)
        {
            return false;
        }
    }

    public void AbortReaders()
    {
        if (stdoutReader != null) { stdoutReader.Dispose(); }
        if (stderrReader != null) { stderrReader.Dispose(); }
    }

    public void Dispose()
    {
        AbortReaders();
        stageWriter.Dispose();
    }
}
'@
}

# A probe invoked from a Make target must start a fresh build owner. Inheriting
# GNU make's jobserver handles/level through PowerShell can detach or misreport
# the measured child on Windows.
$env:MAKEFLAGS = $null
$env:MFLAGS = $null
$env:MAKELEVEL = $null

$processInfo = New-Object System.Diagnostics.ProcessStartInfo
$processInfo.FileName = $Command
$processInfo.Arguments = (($Arguments | ForEach-Object {
    ConvertTo-NativeArgument -Value $_
}) -join ' ')
$processInfo.UseShellExecute = $false
$processInfo.CreateNoWindow = $true
$processInfo.RedirectStandardOutput = $true
$processInfo.RedirectStandardError = $true
$processInfo.EnvironmentVariables["PGY_BUILD_PRESSURE_ACTIVE"] = "1"
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $processInfo
$capture = [BuildPressureOutputCapture]::new(
    [System.IO.Path]::GetFullPath($stagePath)
)
$started = Get-Date
$capture.StartClock()
if (-not $process.Start()) {
    $capture.Dispose()
    throw "failed to start measured build"
}
$capture.Start($process.StandardOutput, $process.StandardError)
$peakWorkingSet = 0.0
$peakPrivate = 0.0
$peakProcessCount = 0
$peakName = ""
$peakTopPrivate = 0.0
$timedOut = $false
$limitExceeded = $false
$phaseStats = @{
    orchestrate = @{ Samples = 0; PeakWorkingSet = 0.0; PeakPrivate = 0.0 }
    compile = @{ Samples = 0; PeakWorkingSet = 0.0; PeakPrivate = 0.0 }
    link = @{ Samples = 0; PeakWorkingSet = 0.0; PeakPrivate = 0.0 }
}
$trackDetachedCompilerWorkers = -not [bool]$RootProcessTreeOnly

function Get-ProcessTreeRows {
    param(
        [int]$RootPid,
        [datetime]$StartedAt,
        [bool]$IncludeDetachedCompilerWorkers
    )

    $all = Get-CimInstance Win32_Process |
        Select-Object ProcessId, ParentProcessId, Name, CommandLine, CreationDate
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

    # The measured root can exit between HasExited and the CIM snapshot. A
    # reused PID must not adopt an unrelated process tree (for example vmmem)
    # and turn that host-wide memory into this build's peak or kill target.
    if (-not $byId.ContainsKey($RootPid)) {
        return @()
    }
    $rootCreatedAt = [datetime]$byId[$RootPid].CreationDate
    if ([Math]::Abs(($rootCreatedAt - $StartedAt).TotalSeconds) -gt 5) {
        return @()
    }

    $result = New-Object System.Collections.Generic.List[object]
    $resultIds = New-Object System.Collections.Generic.HashSet[int]
    $queue = New-Object System.Collections.Generic.Queue[int]
    $queue.Enqueue($RootPid)
    while ($queue.Count -gt 0) {
        $currentPid = $queue.Dequeue()
        if ($byId.ContainsKey($currentPid)) {
            if ($resultIds.Add($currentPid)) {
                $result.Add($byId[$currentPid])
            }
        }
        if ($byParent.ContainsKey($currentPid)) {
            foreach ($child in $byParent[$currentPid]) {
                $queue.Enqueue($child)
            }
        }
    }

    if ($IncludeDetachedCompilerWorkers) {
        # MSYS2/Git Bash fork emulation can reparent native compiler workers in
        # the Win32 process table. The default mode attributes compiler workers
        # created after this isolated probe started. Root-only mode skips this
        # name-based attribution so concurrent Codex tasks remain unowned.
        $detachedToolPattern = `
            '^(cc|gcc|g\+\+|clang|clang\+\+|clang-cl|cc1|cc1plus|lto1|lto-wrapper|collect2|ld|lld|lld-link|pgy|pgy-self-driver|parser_ast_producer|gen[0-9]+|driver_(oracle|seed|gen[0-9]+|c|llvm))(\.exe)?$'
        foreach ($p in $all) {
            if ([string]$p.Name -notmatch $detachedToolPattern) {
                continue
            }
            $createdAt = [datetime]$p.CreationDate
            if ($createdAt -lt $StartedAt.AddSeconds(-1)) {
                continue
            }
            $toolPid = [int]$p.ProcessId
            if ($resultIds.Add($toolPid)) {
                $result.Add($p)
            }
        }
    }
    return $result
}

while (-not $process.HasExited) {
    $rows = Get-ProcessTreeRows -RootPid $process.Id -StartedAt $started `
        -IncludeDetachedCompilerWorkers $trackDetachedCompilerWorkers
    $ownedProcessIds = @($rows | ForEach-Object { [int]$_.ProcessId })
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
        $isCompiler = $processName -match `
            '^(cc|gcc|g\+\+|clang|clang\+\+|clang-cl)(\.exe)?$'
        $isCompileWorker = $processName -match `
            '^(cc1|cc1plus)(\.exe)?$'
        $isLinkWorker = $processName -match `
            '^(lto1|lto-wrapper|collect2|ld|lld|lld-link)(\.exe)?$'
        if ($isLinkWorker) {
            $linkProcCount++
        }
        elseif ($isCompileWorker -or
            ($isCompiler -and $commandLine -match '(^|\s)-c(\s|$)')) {
            $compileProcCount++
        }
        elseif ($isCompiler -and $commandLine -match '(^|\s)-o(\s|$)') {
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
    # CSV is a machine-owned artifact. Locale-aware N1 formatting inserts a
    # thousands comma on large builds and silently changes the column count.
    $line = "{0},{1},{2},{3},{4},{5},{6},{7},{8}" -f `
        $elapsed, $phase, $procs.Count, $compileProcCount, $linkProcCount, `
        $workingSet.ToString("F1", $invariantCulture), `
        $private.ToString("F1", $invariantCulture), $topName, `
        $topPrivate.ToString("F1", $invariantCulture)
    # A live observer may open the CSV without write sharing (for example,
    # Import-Csv on Windows). Treat that short lock as instrumentation
    # contention, not as a reason to abandon the measured build. The retry is
    # deliberately bounded so a persistent ownership problem still fails.
    $sampleWritten = $false
    for ($sampleAttempt = 1; $sampleAttempt -le 20; $sampleAttempt++) {
        try {
            Add-Content -Encoding ASCII -Path $samplePath -Value $line
            $sampleWritten = $true
            break
        }
        catch [System.IO.IOException] {
            if ($sampleAttempt -eq 20) {
                throw
            }
            Start-Sleep -Milliseconds 25
        }
    }
    if (-not $sampleWritten) {
        throw "build-pressure sample append did not complete"
    }

    if ($workingSet -gt $peakWorkingSet) {
        $peakWorkingSet = $workingSet
    }
    if ($private -gt $peakPrivate) {
        $peakPrivate = $private
        $peakProcessCount = $procs.Count
        $peakName = $topName
        $peakTopPrivate = $topPrivate
    }

    if ($peakPrivate -gt $LimitMB -or $peakWorkingSet -gt $LimitMB) {
        $limitExceeded = $true
        if ($StopOnLimit) {
            foreach ($id in ($ownedProcessIds | Sort-Object -Descending)) {
                Stop-Process -Id $id -Force -ErrorAction SilentlyContinue
            }
            break
        }
    }

    if ($TimeoutSec -gt 0 -and ((Get-Date) - $started).TotalSeconds -ge $TimeoutSec) {
        $timedOut = $true
        foreach ($id in ($ownedProcessIds | Sort-Object -Descending)) {
            Stop-Process -Id $id -Force -ErrorAction SilentlyContinue
        }
        break
    }

    Start-Sleep -Milliseconds $IntervalMs
    $process.Refresh()
}

$rootExitComplete = $process.WaitForExit($OutputDrainTimeoutMs)
if (-not $rootExitComplete) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    $rootExitComplete = $process.WaitForExit($OutputDrainTimeoutMs)
}
$outputCaptureComplete = $capture.WaitForCompletion($OutputDrainTimeoutMs)
$outputCaptureStopped = $outputCaptureComplete
if (-not $outputCaptureComplete) {
    $capture.AbortReaders()
    $outputCaptureStopped = $capture.WaitForCompletion($OutputDrainTimeoutMs)
}
$stdoutText = $capture.StandardOutputText()
$stderrText = $capture.StandardErrorText()
$lastObservedStage = $capture.LastObservedStage()
$observedStageCount = $capture.ObservedStageCount()
if ($outputCaptureStopped) { $capture.Dispose() }
$exitCode = if ($rootExitComplete) { [int]$process.ExitCode } else { -1 }
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText(
    [System.IO.Path]::GetFullPath($stdoutPath),
    $stdoutText,
    $utf8NoBom
)
[System.IO.File]::WriteAllText(
    [System.IO.Path]::GetFullPath($stderrPath),
    $stderrText,
    $utf8NoBom
)
if ($timedOut) {
    $exitCode = 124
}

$elapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
$peakWorkingSetGiB = [math]::Round($peakWorkingSet / 1024.0, 3)
$peakPrivateGiB = [math]::Round($peakPrivate / 1024.0, 3)
$attentionLimitMB = $LimitMB * ($AttentionPercent / 100.0)
$attentionRequired = $peakPrivate -ge $attentionLimitMB
$summary = [ordered]@{
    schema = "pgy.build-pressure.v2"
    label = $Label
    exit_code = $exitCode
    elapsed_ms = $elapsedMs
    interval_ms = $IntervalMs
    peak_working_set_mb = [math]::Round($peakWorkingSet, 1)
    peak_private_mb = [math]::Round($peakPrivate, 1)
    peak_working_set_gib = $peakWorkingSetGiB
    peak_private_gib = $peakPrivateGiB
    peak_processes = $peakProcessCount
    top_private_process = $peakName
    top_private_mb = [math]::Round($peakTopPrivate, 1)
    limit_mb = $LimitMB
    attention_percent = $AttentionPercent
    attention_limit_gib = [math]::Round($attentionLimitMB / 1024.0, 3)
    attention_required = $attentionRequired
    stop_on_limit = [bool]$StopOnLimit
    limit_exceeded = $limitExceeded
    detached_compiler_worker_tracking = $trackDetachedCompilerWorkers
    output_capture_complete = $outputCaptureComplete
    observed_stage_count = $observedStageCount
    last_observed_stage = $lastObservedStage
    phases = [ordered]@{
        orchestrate = $phaseStats.orchestrate
        compile = $phaseStats.compile
        link = $phaseStats.link
    }
    samples = $samplePath
    stages = $stagePath
}
$summary | ConvertTo-Json -Depth 4 | Set-Content -Encoding ASCII -Path $summaryPath

Write-Output ("[build-pressure] label={0} exit={1} elapsed_ms={2} peak_working_set_gib={3:N3} peak_private_gib={4:N3} attention_required={5} summary={6}" -f `
    $Label, $exitCode, $elapsedMs, $peakWorkingSetGiB, $peakPrivateGiB, `
    $attentionRequired, $summaryPath)

if ($timedOut) {
    [Console]::Error.WriteLine(("[build-pressure] timed out after {0}s" -f $TimeoutSec))
}

if ($attentionRequired -and -not $limitExceeded) {
    [Console]::Error.WriteLine(("[build-pressure] peak crossed the {0}% attention threshold ({1:N3} GiB)" -f $AttentionPercent, ($attentionLimitMB / 1024.0)))
}

if ($limitExceeded) {
    [Console]::Error.WriteLine(("[build-pressure] peak exceeded limit {0} MB; this is a compiler/build memory bug until proven otherwise" -f $LimitMB))
    exit 88
}

exit $exitCode
