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
    [string[]]$InputPaths = @(),
    [string[]]$ExecutablePaths = @(),
    [string]$OutDir = ".tmp/build-pressure"
)

$ErrorActionPreference = "Stop"
$invariantCulture = [Globalization.CultureInfo]::InvariantCulture

if ($AttentionPercent -lt 1 -or $AttentionPercent -gt 99) {
    throw "AttentionPercent must be between 1 and 99"
}
if ($LimitMB -le 0 -or $IntervalMs -le 0 -or $TimeoutSec -lt 0 -or $OutputDrainTimeoutMs -le 0) {
    throw "pressure limits/interval/drain must be positive and timeout cannot be negative"
}
if ([string]::IsNullOrWhiteSpace($Label)) { throw "pressure label cannot be empty" }

# Bind the bytes actually named by the caller, not a guessed source snapshot.
# The baseline owner still decides which paths constitute the complete input.
function Get-PressureFileBinding {
    param(
        [string]$Path,
        [string]$Role,
        [switch]$AllowUnavailable
    )

    $resolved = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
    try {
        $file = Get-Item -LiteralPath $resolved -ErrorAction Stop
        if ($file -isnot [System.IO.FileInfo]) {
            throw "pressure binding requires a file: $resolved"
        }
        $digest = Get-FileHash -LiteralPath $resolved -Algorithm SHA256 -ErrorAction Stop
        return [ordered]@{
            role = $Role
            path = $file.FullName
            sha256 = $digest.Hash.ToLowerInvariant()
            bytes = $file.Length
            state = "present"
        }
    }
    catch {
        if (-not $AllowUnavailable) { throw }
        # Preserve an invalid receipt after the child exits; never accept a
        # deleted/unreadable input as unchanged or lose the command's result.
        return [ordered]@{
            role = $Role
            path = $resolved
            sha256 = $null
            bytes = $null
            state = $_.Exception.GetType().Name
        }
    }
}

$measurementCwd = (Get-Location).ProviderPath
$commandPath = (Get-Command -Name ([WildcardPattern]::Escape($Command)) -CommandType Application -ErrorAction Stop |
    Select-Object -First 1).Source
$bindingsBefore = @(
    Get-PressureFileBinding -Path $commandPath -Role "command"
    Get-PressureFileBinding -Path $PSCommandPath -Role "probe"
    foreach ($inputPath in $InputPaths) {
        Get-PressureFileBinding -Path $inputPath -Role "input"
    }
    foreach ($executablePath in $ExecutablePaths) {
        Get-PressureFileBinding -Path $executablePath -Role "executable"
    }
)

# Make's stable labels may be measured repeatedly. Choose a fresh evidence
# lifetime by default; an explicit destination never gains overwrite authority.
if (-not $PSBoundParameters.ContainsKey("OutDir")) {
    $OutDir = Join-Path $OutDir ("run-" + [Guid]::NewGuid().ToString("N"))
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$safeLabel = ($Label -replace '[^A-Za-z0-9_.-]', '-')
$samplePath = Join-Path $OutDir "$safeLabel.samples.csv"
$summaryPath = Join-Path $OutDir "$safeLabel.summary.json"
$stdoutPath = Join-Path $OutDir "$safeLabel.stdout.log"
$stderrPath = Join-Path $OutDir "$safeLabel.stderr.log"
$stagePath = Join-Path $OutDir "$safeLabel.stages.csv"

# Reserve every output with exclusive creation, including the summary. A
# repeated/sanitized label must not destroy an earlier run's evidence.
$artifactPaths = @($samplePath, $stagePath, $stdoutPath, $stderrPath, $summaryPath)
foreach ($artifactPath in $artifactPaths) {
    if (Test-Path -LiteralPath $artifactPath) {
        throw "pressure output already exists; choose a fresh label/OutDir: $artifactPath"
    }
}
$artifactStreams = @{}
$capture = $null
$process = $null
$processStarted = $false
$ownedRows = @()
try {
foreach ($artifactPath in $artifactPaths) {
    $artifactStreams[$artifactPath] = [IO.File]::Open(
        [IO.Path]::GetFullPath($artifactPath), [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::Read)
}
$sampleHeader = [Text.Encoding]::ASCII.GetBytes(
    "elapsed_ms,phase,proc_count,compile_proc_count,link_proc_count,working_set_mb,private_mb,max_proc,top_private_mb`r`n")
$artifactStreams[$samplePath].Write($sampleHeader, 0, $sampleHeader.Length)
$artifactStreams[$samplePath].Dispose()

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
    private const int MaxStageCharacters = 4096;
    private static readonly string[] StagePrefixes = new string[] {
        "[driver-pressure-stage]", "[semantic-body-type-stage]",
        "[semantic-initializer-stage]", "[codegen-view-stage]",
        "[codegen-pressure-stage]"
    };
    private readonly Stream stdoutLog;
    private readonly Stream stderrLog;
    private readonly StringBuilder stdoutStageLine = new StringBuilder();
    private readonly StringBuilder stderrStageLine = new StringBuilder();
    private readonly Stopwatch clock = new Stopwatch();
    private readonly StreamWriter stageWriter;
    private Stream stdoutReader;
    private Stream stderrReader;
    private Task stdoutTask;
    private Task stderrTask;
    private volatile bool stdoutEof;
    private volatile bool stderrEof;
    private string lastObservedStage = "";
    private int observedStageCount;
    private string failure = "";
    private int maxPendingStageCharacters;

    public BuildPressureOutputCapture(Stream outputLog, Stream errorLog, Stream stages)
    {
        stdoutLog = outputLog;
        stderrLog = errorLog;
        stageWriter = new StreamWriter(stages, new UTF8Encoding(false), 4096, true);
        stageWriter.AutoFlush = true;
        stageWriter.WriteLine("observed_elapsed_ms,stream,stage");
    }

    public void StartClock()
    {
        clock.Restart();
    }

    public void Start(StreamReader output, StreamReader error)
    {
        stdoutReader = output.BaseStream;
        stderrReader = error.BaseStream;
        stdoutTask = PumpAsync(
            stdoutReader, stdoutLog, stdoutStageLine, "stdout", true);
        stderrTask = PumpAsync(
            stderrReader, stderrLog, stderrStageLine, "stderr", false);
    }

    private async Task PumpAsync(
        Stream reader,
        Stream log,
        StringBuilder stageLine,
        string stream,
        bool isStdout)
    {
        byte[] buffer = new byte[4096];
        char[] decoded = new char[Encoding.UTF8.GetMaxCharCount(buffer.Length)];
        Decoder decoder = Encoding.UTF8.GetDecoder();
        bool discardStageLine = false;
        try
        {
            while (true)
            {
                int count = await reader.ReadAsync(
                    buffer, 0, buffer.Length).ConfigureAwait(false);
                if (count == 0)
                {
                    int trailing = decoder.GetChars(buffer, 0, 0, decoded, 0, true);
                    CaptureStageLines(stream, stageLine, decoded, trailing, ref discardStageLine);
                    if (!discardStageLine) { RecordTrailingStage(stream, stageLine); }
                    await log.FlushAsync().ConfigureAwait(false);
                    if (isStdout) { stdoutEof = true; }
                    else { stderrEof = true; }
                    return;
                }
                // Raw bytes, including CRLF, invalid UTF-8 and no final LF,
                // belong to the artifact; decoding is only for stage metadata.
                await log.WriteAsync(buffer, 0, count).ConfigureAwait(false);
                int chars = decoder.GetChars(buffer, 0, count, decoded, 0, false);
                CaptureStageLines(stream, stageLine, decoded, chars, ref discardStageLine);
            }
        }
        catch (ObjectDisposedException error)
        {
            RecordFailure(stream + ":" + error.GetType().Name);
        }
        catch (IOException error)
        {
            RecordFailure(stream + ":" + error.GetType().Name);
        }
    }

    private void CaptureStageLines(
        string stream,
        StringBuilder pending,
        char[] buffer,
        int count,
        ref bool discard)
    {
        for (int i = 0; i < count; i++)
        {
            char current = buffer[i];
            if (current == '\n')
            {
                if (!discard) { RecordTrailingStage(stream, pending); }
                pending.Clear();
                discard = false;
            }
            else if (!discard)
            {
                if (pending.Length == MaxStageCharacters)
                {
                    RecordFailure(stream + ":stage line exceeds 4096 characters");
                    pending.Clear();
                    discard = true;
                    continue;
                }
                pending.Append(current);
                if (pending.Length <= 30)
                {
                    string prefix = pending.ToString();
                    bool possible = false;
                    foreach (string candidate in StagePrefixes)
                    {
                        if (candidate.StartsWith(prefix, StringComparison.Ordinal) ||
                            prefix.StartsWith(candidate, StringComparison.Ordinal))
                        { possible = true; break; }
                    }
                    if (!possible) { pending.Clear(); discard = true; }
                }
                lock (stageWriter)
                {
                    if (pending.Length > maxPendingStageCharacters)
                    { maxPendingStageCharacters = pending.Length; }
                }
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

    private void RecordFailure(string reason)
    {
        lock (stageWriter) { if (failure.Length == 0) { failure = reason; } }
    }

    public string Failure()
    {
        lock (stageWriter) { return failure; }
    }

    public int MaxPendingStageCharacters()
    {
        lock (stageWriter) { return maxPendingStageCharacters; }
    }

    public bool WaitForCompletion(int timeoutMs)
    {
        return WaitForReadersStopped(timeoutMs) && stdoutEof && stderrEof && Failure().Length == 0;
    }

    public bool WaitForReadersStopped(int timeoutMs)
    {
        if (stdoutTask == null || stderrTask == null)
        {
            return false;
        }
        try
        {
            return Task.WaitAll(
                new Task[] { stdoutTask, stderrTask }, timeoutMs);
        }
        catch (AggregateException error)
        {
            RecordFailure("capture task:" + error.GetType().Name);
            return stdoutTask.IsCompleted && stderrTask.IsCompleted;
        }
    }

    public void AbortReaders()
    {
        if (!stdoutEof || !stderrEof) { RecordFailure("capture aborted before EOF"); }
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
$processInfo.FileName = $commandPath
$processInfo.WorkingDirectory = $measurementCwd
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
    $artifactStreams[$stdoutPath], $artifactStreams[$stderrPath], $artifactStreams[$stagePath]
)
$started = Get-Date
$capture.StartClock()
if (-not $process.Start()) {
    $capture.Dispose()
    throw "failed to start measured build"
}
$processStarted = $true
$rootProcessCreation = $process.StartTime
$capture.Start($process.StandardOutput, $process.StandardError)
$peakWorkingSet = 0.0
$peakPrivate = 0.0
$peakProcessCount = 0
$peakName = ""
$peakTopPrivate = 0.0
$timedOut = $false
$limitExceeded = $false
$unattributedProcessSeen = $false
$processIdentityMismatchSeen = $false
$memorySampleCount = 0
$captureAbortRequested = $false
$phaseStats = @{
    orchestrate = @{ Samples = 0; PeakWorkingSet = 0.0; PeakPrivate = 0.0 }
    compile = @{ Samples = 0; PeakWorkingSet = 0.0; PeakPrivate = 0.0 }
    link = @{ Samples = 0; PeakWorkingSet = 0.0; PeakPrivate = 0.0 }
}
$trackDetachedCompilerWorkers = -not [bool]$RootProcessTreeOnly

function Test-PressureProcessIdentity {
    param([datetime]$ActualCreation, [datetime]$ExpectedCreation)
    # CIM exposes microseconds; Process.StartTime exposes 100 ns FILETIME.
    # Normalize precision, not a tolerance window that can admit another PID lifetime.
    $actualTicks = $ActualCreation.ToUniversalTime().Ticks
    $expectedTicks = $ExpectedCreation.ToUniversalTime().Ticks
    return ($actualTicks - ($actualTicks % 10)) -eq ($expectedTicks - ($expectedTicks % 10))
}

function Get-ProcessTreeRows {
    param(
        [int]$RootPid,
        [datetime]$StartedAt,
        [datetime]$ExpectedRootCreation,
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
    if (-not (Test-PressureProcessIdentity -ActualCreation $rootCreatedAt -ExpectedCreation $ExpectedRootCreation)) {
        return @()
    }

    $result = New-Object System.Collections.Generic.List[object]
    $resultIds = New-Object System.Collections.Generic.HashSet[int]
    $queue = New-Object System.Collections.Generic.Queue[int]
    $queue.Enqueue($RootPid)
    while ($queue.Count -gt 0) {
        $currentPid = $queue.Dequeue()
        if ($resultIds.Contains($currentPid)) { continue }
        if ($byId.ContainsKey($currentPid)) {
            if ($resultIds.Add($currentPid)) {
                $byId[$currentPid] | Add-Member -NotePropertyName PressureOwned -NotePropertyValue $true -Force
                $result.Add($byId[$currentPid])
            }
        }
        if ($byParent.ContainsKey($currentPid)) {
            foreach ($child in $byParent[$currentPid]) {
                if ([datetime]$byId[$child].CreationDate -lt [datetime]$byId[$currentPid].CreationDate) {
                    continue # A recycled parent PID cannot own an older process.
                }
                $queue.Enqueue($child)
            }
        }
    }

    if ($IncludeDetachedCompilerWorkers) {
        # MSYS2/Git Bash fork emulation can reparent native compiler workers in
        # the Win32 process table. Name/time alone are NOT run ownership.
        # Report candidates as incomplete attribution, never sum their memory
        # into this run or authorize their termination.
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
                $p | Add-Member -NotePropertyName PressureOwned -NotePropertyValue $false -Force
                $result.Add($p)
            }
        }
    }
    return $result
}

function Stop-PressureOwnedProcesses {
    param([object[]]$Rows, [Diagnostics.Process]$RootProcess, [int]$DrainTimeoutMs)
    try {
        foreach ($row in ($Rows | Where-Object PressureOwned | Sort-Object CreationDate -Descending)) {
            if ([int]$row.ProcessId -eq $RootProcess.Id) { continue }
            $child = $null
            try {
                $child = [Diagnostics.Process]::GetProcessById([int]$row.ProcessId)
                [void]$child.Handle # Pin the process object before checking its identity.
                $expectedStart = ([datetime]$row.CreationDate).ToUniversalTime()
                if (-not (Test-PressureProcessIdentity -ActualCreation $child.StartTime -ExpectedCreation $expectedStart)) {
                    throw "pressure child identity changed; refusing PID-based termination"
                }
                if (-not $child.HasExited) { $child.Kill() }
                if (-not $child.WaitForExit($DrainTimeoutMs)) { throw "owned pressure child did not exit" }
            }
            catch [ArgumentException] {
                Write-Verbose "Selected child already exited before termination."
            }
            catch [InvalidOperationException] {
                if ($null -eq $child -or -not $child.HasExited) { throw }
                Write-Verbose "Selected child exited during identity validation."
            }
            finally { if ($null -ne $child) { $child.Dispose() } }
        }
    }
    finally {
        # This is the original held process object, not a reopened root PID.
        if (-not $RootProcess.HasExited) { $RootProcess.Kill() }
        if (-not $RootProcess.WaitForExit($DrainTimeoutMs)) { throw "measured pressure root did not exit" }
    }
}

while (-not $process.HasExited) {
    if ($capture.Failure().Length -gt 0) {
        $captureAbortRequested = $true
        Stop-PressureOwnedProcesses -Rows $ownedRows -RootProcess $process -DrainTimeoutMs $OutputDrainTimeoutMs
        break
    }
    $rows = Get-ProcessTreeRows -RootPid $process.Id -StartedAt $started `
        -ExpectedRootCreation $rootProcessCreation `
        -IncludeDetachedCompilerWorkers $trackDetachedCompilerWorkers
    $ownedRows = @($rows | Where-Object PressureOwned)
    if (@($rows | Where-Object { -not $_.PressureOwned }).Count -gt 0) {
        $unattributedProcessSeen = $true
    }
    $procs = @()
    $compileProcCount = 0
    $linkProcCount = 0
    foreach ($row in $ownedRows) {
        $p = $null
        try {
            $p = [Diagnostics.Process]::GetProcessById([int]$row.ProcessId)
            [void]$p.Handle
            if (-not (Test-PressureProcessIdentity -ActualCreation $p.StartTime -ExpectedCreation ([datetime]$row.CreationDate))) {
                $processIdentityMismatchSeen = $true
                $p.Dispose()
                continue
            }
            $procs += $p
        }
        catch [ArgumentException] {
            Write-Verbose "Selected process exited before its memory sample."
            if ($null -ne $p) { $p.Dispose() }
            continue
        }
        catch [InvalidOperationException] {
            if ($null -eq $p -or -not $p.HasExited) { throw }
            $p.Dispose()
            Write-Verbose "Selected process exited during sample identity validation."
            continue
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
    if ($procs.Count -gt 0) { $memorySampleCount++ }

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
    foreach ($p in $procs) { $p.Dispose() }

    if ($peakPrivate -gt $LimitMB -or $peakWorkingSet -gt $LimitMB) {
        $limitExceeded = $true
        if ($StopOnLimit) {
            Stop-PressureOwnedProcesses -Rows $ownedRows -RootProcess $process -DrainTimeoutMs $OutputDrainTimeoutMs
            break
        }
    }

    if ($TimeoutSec -gt 0 -and ((Get-Date) - $started).TotalSeconds -ge $TimeoutSec) {
        $timedOut = $true
        Stop-PressureOwnedProcesses -Rows $ownedRows -RootProcess $process -DrainTimeoutMs $OutputDrainTimeoutMs
        break
    }

    Start-Sleep -Milliseconds $IntervalMs
    $process.Refresh()
}

$rootExitComplete = $process.WaitForExit($OutputDrainTimeoutMs)
if (-not $rootExitComplete) {
    Stop-PressureOwnedProcesses -Rows $ownedRows -RootProcess $process -DrainTimeoutMs $OutputDrainTimeoutMs
    $rootExitComplete = $process.WaitForExit($OutputDrainTimeoutMs)
}
$outputCaptureComplete = $capture.WaitForCompletion($OutputDrainTimeoutMs)
$outputCaptureStopped = $outputCaptureComplete
if (-not $outputCaptureComplete) {
    $capture.AbortReaders()
    $outputCaptureStopped = $capture.WaitForReadersStopped($OutputDrainTimeoutMs)
}
$captureFailure = $capture.Failure()
$maxPendingStageCharacters = $capture.MaxPendingStageCharacters()
$lastObservedStage = $capture.LastObservedStage()
$observedStageCount = $capture.ObservedStageCount()
if ($outputCaptureStopped) { $capture.Dispose() }
$exitCode = if ($rootExitComplete) { [int]$process.ExitCode } else { -1 }
$commandExitCode = $exitCode
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
if ($timedOut) {
    $exitCode = 124
}

$fileBindings = @(
    foreach ($before in $bindingsBefore) {
        $after = Get-PressureFileBinding -Path $before.path -Role $before.role -AllowUnavailable
        $stable = $after.state -eq "present" -and
            $before.sha256 -eq $after.sha256 -and $before.bytes -eq $after.bytes
        [ordered]@{
            role = $before.role
            path = $before.path
            before = $before
            after = $after
            unchanged = $stable
        }
    }
)
$bindingChanged = @($fileBindings | Where-Object { -not $_.unchanged }).Count -gt 0
if ($bindingChanged -and $exitCode -eq 0) {
    $exitCode = 89
}
if ($limitExceeded) { $exitCode = 88 }
if ($captureAbortRequested -and -not $limitExceeded -and -not $timedOut) { $exitCode = 90 }
if ((-not $outputCaptureComplete -or $unattributedProcessSeen -or $processIdentityMismatchSeen) -and $exitCode -eq 0) {
    $exitCode = 90
}

$elapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
$peakWorkingSetGiB = [math]::Round($peakWorkingSet / 1024.0, 3)
$peakPrivateGiB = [math]::Round($peakPrivate / 1024.0, 3)
$attentionLimitMB = $LimitMB * ($AttentionPercent / 100.0)
$attentionRequired = $peakPrivate -ge $attentionLimitMB
$summary = [ordered]@{
    schema = "pgy.build-pressure.v3"
    label = $Label
    exit_code = $exitCode
    elapsed_ms = $elapsedMs
    interval_ms = $IntervalMs
    memory_peak_scope = "observed-samples-only"
    memory_sample_count = $memorySampleCount
    memory_measurement_state = if ($memorySampleCount -gt 0) { "sampled" } else { "UNOBSERVED" }
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
    process_observation_scope = "validated-root-tree"
    detached_worker_attribution_complete = -not $unattributedProcessSeen
    process_identity_mismatch_seen = $processIdentityMismatchSeen
    output_capture_complete = $outputCaptureComplete
    output_capture_failure = $captureFailure
    capture_abort_requested = $captureAbortRequested
    max_pending_stage_characters = $maxPendingStageCharacters
    observed_stage_count = $observedStageCount
    last_observed_stage = $lastObservedStage
    phases = [ordered]@{
        orchestrate = $phaseStats.orchestrate
        compile = $phaseStats.compile
        link = $phaseStats.link
    }
    execution = [ordered]@{
        cwd = $measurementCwd
        command = $Command
        command_path = $commandPath
        argv = @($Arguments)
        command_exit_code = $commandExitCode
        file_bindings = $fileBindings
        binding_changed = $bindingChanged
        input_change_scope = "before-after-only"
        input_scope = if ($InputPaths.Count -gt 0) { "declared-paths-only" } else { "unbound" }
        executable_scope = if ($ExecutablePaths.Count -gt 0) { "command-and-declared-paths" } else { "command-only" }
        heap_counter_state = "UNMEASURED"
        heap_peak_live_bytes = $null
        heap_total_allocated_bytes = $null
    }
    samples = $samplePath
    stages = $stagePath
}
$summaryBytes = $utf8NoBom.GetBytes(($summary | ConvertTo-Json -Depth 7))
$artifactStreams[$summaryPath].Write($summaryBytes, 0, $summaryBytes.Length)
$artifactStreams[$summaryPath].Flush()

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

if ($bindingChanged) {
    [Console]::Error.WriteLine("[build-pressure] declared input/tool/executable bytes changed or became unavailable; receipt is not a stable baseline")
}
if (-not $outputCaptureComplete) {
    [Console]::Error.WriteLine("[build-pressure] output capture incomplete/failed; receipt is invalid: " + $captureFailure)
}
if ($unattributedProcessSeen) {
    [Console]::Error.WriteLine("[build-pressure] detached compiler candidate lacks run ownership; root-tree metrics are incomplete")
}
if ($processIdentityMismatchSeen) {
    [Console]::Error.WriteLine("[build-pressure] sampled process creation identity changed; root-tree metrics are incomplete")
}

exit $exitCode
}
finally {
    # A sampling/hash/log failure must not abandon the command this run owns.
    if ($null -ne $process) {
        try {
            if ($processStarted -and -not $process.HasExited) {
                Stop-PressureOwnedProcesses -Rows $ownedRows -RootProcess $process -DrainTimeoutMs $OutputDrainTimeoutMs
            }
        }
        finally { $process.Dispose() }
    }
    if ($null -ne $capture) {
        $capture.AbortReaders()
        if (-not $capture.WaitForReadersStopped($OutputDrainTimeoutMs)) {
            throw "pressure capture tasks did not stop within the drain budget"
        }
        $capture.Dispose()
    }
    foreach ($artifactStream in $artifactStreams.Values) { $artifactStream.Dispose() }
}
