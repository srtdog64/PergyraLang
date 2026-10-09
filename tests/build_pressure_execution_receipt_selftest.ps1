param(
    [string]$CaseFile = "",
    [switch]$ReceiptChild,
    [string]$Payload = "",
    [string]$ChangeInput = "",
    [string]$ChildReceiptPath = "",
    [int]$ChildExitCode = 0,
    [int]$ChildSleepMs = 600,
    [switch]$LongBinaryOutput,
    [switch]$OversizedStage
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$probe = Join-Path $repoRoot "scripts/measure_build_pressure.ps1"
$testRoot = Join-Path $repoRoot ".tmp/build-pressure-receipt-selftest"
$utf8 = New-Object System.Text.UTF8Encoding($false)

if ($ReceiptChild) {
    foreach ($changedPath in @($ChangeInput, $ChildReceiptPath)) {
        if ($changedPath.Length -eq 0) { continue }
        $target = [System.IO.Path]::GetFullPath($changedPath)
        $boundary = [System.IO.Path]::GetFullPath($testRoot) + [System.IO.Path]::DirectorySeparatorChar
        if (-not $target.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)) {
            throw "receipt self-test may change only its own fixture: $target"
        }
        [System.IO.File]::WriteAllText($target, "changed by the measured child", $utf8)
    }
    [Console]::WriteLine("RECEIPT_CHILD:" + [Convert]::ToBase64String($utf8.GetBytes($Payload)))
    if ($LongBinaryOutput) {
        [Console]::Out.Flush()
        $raw = [Console]::OpenStandardOutput()
        $begin = $utf8.GetBytes("[driver-pressure-stage]fixture:begin`r`n")
        $raw.Write($begin,0,$begin.Length)
        $block = [Text.Encoding]::ASCII.GetBytes(('X' * 4096))
        for ($i = 0; $i -lt 1024; $i++) { $raw.Write($block,0,$block.Length) }
        $binary = [byte[]]@(255,0,13,10)
        $raw.Write($binary,0,$binary.Length)
        $end = $utf8.GetBytes('[driver-pressure-stage]fixture:end')
        $raw.Write($end,0,$end.Length)
        $raw.Flush()
        [Console]::Error.WriteLine('[semantic-body-type-stage]fixture:stderr')
    }
    if ($OversizedStage) {
        [Console]::WriteLine('[driver-pressure-stage]' + ('Y' * 4097))
    }
    Start-Sleep -Milliseconds $ChildSleepMs
    exit $ChildExitCode
}

if ($CaseFile.Length -gt 0) {
    $config = Get-Content -LiteralPath $CaseFile -Raw -Encoding UTF8 | ConvertFrom-Json
    $caseProbe = $probe
    if ($config.capture_fault -or $config.unattributed_worker -or $config.identity_sample_mismatch) {
        $caseProbe = Join-Path (Split-Path -Parent $config.output) 'owner-fixture-probe.ps1'
        if (-not ([IO.Path]::GetFullPath($caseProbe)).StartsWith(
                ([IO.Path]::GetFullPath($testRoot) + [IO.Path]::DirectorySeparatorChar),
                [StringComparison]::OrdinalIgnoreCase)) {
            throw 'owner fault injection must stay inside the self-test fixture'
        }
    }
    $pressureArguments = @{
        Label = $config.label; Command = $config.command; Arguments = @($config.argv)
        InputPaths = @($config.inputs); ExecutablePaths = @($config.executables)
        RootProcessTreeOnly = $true; StopOnLimit = $true; LimitMB = $config.limit_mb
        TimeoutSec = 10; IntervalMs = $config.interval_ms; OutputDrainTimeoutMs = $config.drain_ms
    }
    if (-not $config.default_outdir) { $pressureArguments.OutDir = $config.output }
    & $caseProbe @pressureArguments
    exit $LASTEXITCODE
}

function Assert-PressureReceipt {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw "[build-pressure-receipt] $Message" }
}

function Test-PressureProcessSelection {
    $tokens = $null; $errors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($probe,[ref]$tokens,[ref]$errors)
    Assert-PressureReceipt ($errors.Count -eq 0) 'measurement owner did not parse'
    foreach ($name in @('Test-PressureProcessIdentity','Get-ProcessTreeRows')) {
        $definition = $ast.Find({param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
        },$true)
        Assert-PressureReceipt ($null -ne $definition) "process ownership owner missing: $name"
        . ([scriptblock]::Create($definition.Extent.Text))
    }
    $fixtureStartedAt = [datetime]'2026-10-09T00:00:00Z'
    $fixtureRows = @(
        [pscustomobject]@{ProcessId=424240;ParentProcessId=0;Name='fixture.exe';CommandLine='';CreationDate=$fixtureStartedAt},
        [pscustomobject]@{ProcessId=424241;ParentProcessId=424240;Name='cc1.exe';CommandLine='';CreationDate=$fixtureStartedAt.AddMilliseconds(1)},
        [pscustomobject]@{ProcessId=424242;ParentProcessId=42;Name='cc1.exe';CommandLine='';CreationDate=$fixtureStartedAt.AddMilliseconds(2)},
        [pscustomobject]@{ProcessId=424243;ParentProcessId=424240;Name='old-fixture.exe';CommandLine='';CreationDate=$fixtureStartedAt.AddSeconds(-30)}
    )
    function Get-CimInstance { param($ClassName) return $fixtureRows }
    $rows = @(Get-ProcessTreeRows -RootPid 424240 -StartedAt $fixtureStartedAt `
        -ExpectedRootCreation $fixtureStartedAt -IncludeDetachedCompilerWorkers $true)
    $owned = @($rows | Where-Object PressureOwned | ForEach-Object ProcessId)
    Assert-PressureReceipt (($owned -join ',') -eq '424240,424241') 'unrelated/reused-parent worker acquired termination authority'
    Assert-PressureReceipt (@($rows | Where-Object { $_.ProcessId -eq 424242 -and -not $_.PressureOwned }).Count -eq 1) `
        'detached candidate was silently dropped or promoted to owned'
    $rootOnly = @(Get-ProcessTreeRows -RootPid 424240 -StartedAt $fixtureStartedAt `
        -ExpectedRootCreation $fixtureStartedAt -IncludeDetachedCompilerWorkers $false)
    Assert-PressureReceipt ($rootOnly.Count -eq 2) 'root-only observation adopted an unrelated worker'
    $reused = @(Get-ProcessTreeRows -RootPid 424240 -StartedAt $fixtureStartedAt `
        -ExpectedRootCreation $fixtureStartedAt.AddMilliseconds(-1) -IncludeDetachedCompilerWorkers $true)
    Assert-PressureReceipt ($reused.Count -eq 0) 'nearby but different root creation time was accepted'
    Assert-PressureReceipt (Test-PressureProcessIdentity -ActualCreation $fixtureStartedAt.AddTicks(4) `
        -ExpectedCreation $fixtureStartedAt) 'CIM/FILETIME precision normalization refused the same identity'
    Write-Output '[build-pressure-receipt] PASS synthetic ownership/creation controls; no real worker terminated'
}

Test-PressureProcessSelection
$hostExe = (Get-Process -Id $PID).Path
$runRoot = Join-Path $testRoot ("run-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $runRoot | Out-Null
$unicodeName = [string][char]0xD14C + [char]0xC2A4 + [char]0xD2B8
$payload = 'space "quoted" ' + $unicodeName + ' trailing\'
$cases = @("bound", "unbound", "input-drift", "executable-drift", "command-failure",
    "failure-and-drift", "pressure-limit", "missing-input", "wildcard-command",
    "capture-fault", "failure-and-capture-fault", "occupied-stdout", "occupied-summary",
    "long-binary-output", "oversized-stage", "invalid-interval", "invalid-drain",
    "unattributed-worker", "sample-identity-mismatch", "default-output-first", "default-output-second")
$defaultWorkingDirectory = Join-Path $runRoot 'default-repeated'
New-Item -ItemType Directory -Path $defaultWorkingDirectory | Out-Null
$firstDefaultBindings = @()
$firstDefaultDirectory = $null

foreach ($case in $cases) {
    $caseRoot = Join-Path $runRoot $case
    New-Item -ItemType Directory -Path $caseRoot | Out-Null
    $input = Join-Path $caseRoot ("fixed " + $unicodeName + ".txt")
    $secondInput = Join-Path $caseRoot "second input.txt"
    $declaredExe = Join-Path $caseRoot "declared-executable.exe"
    $childReceiptPath = Join-Path $caseRoot "child-ran.txt"
    [System.IO.File]::WriteAllText($input, "fixed input bytes", $utf8)
    [System.IO.File]::WriteAllText($secondInput, "second input bytes", $utf8)
    Copy-Item -LiteralPath $hostExe -Destination $declaredExe
    $expectedInputHash = (Get-FileHash -LiteralPath $input -Algorithm SHA256).Hash.ToLowerInvariant()
    $expectedExeHash = (Get-FileHash -LiteralPath $declaredExe -Algorithm SHA256).Hash.ToLowerInvariant()
    $argv = @("-NoProfile", "-NonInteractive", "-File", $PSCommandPath,
        "-ReceiptChild", "-Payload", $payload, "-ChildReceiptPath", $childReceiptPath)
    $inputs = @($input, $secondInput)
    $limitMB = 3072
    $command = $hostExe
    $intervalMs = 100
    $drainMs = 5000
    $captureFault = $case -in @('capture-fault','failure-and-capture-fault')
    if ($captureFault -or $case -in @('unattributed-worker','sample-identity-mismatch')) {
        # Fault only a generated copy; synthetic metadata never becomes a real
        # process-start or shared-worker termination probe.
        $faultSource = [IO.File]::ReadAllText($probe)
        if ($captureFault) {
            $needle = if ($case -eq 'failure-and-capture-fault') { 'if (count == 0)' } else { 'int count = await reader.ReadAsync(' }
            Assert-PressureReceipt ($faultSource.Contains($needle)) 'capture fault site disappeared'
            $faultGuard = if ($case -eq 'failure-and-capture-fault') { 'if (count == 0 && isStdout)' } else { 'if (isStdout)' }
            $faultSource = $faultSource.Replace($needle,
                $faultGuard + ' { throw new IOException("controlled capture fault"); }' + "`n" + $needle)
            $drainMs = 100
        }
        elseif ($case -eq 'unattributed-worker') {
            $needle = 'return $result'
            Assert-PressureReceipt ($faultSource.Contains($needle)) 'process selection fault site disappeared'
            $faultSource = $faultSource.Replace($needle,
                '$result.Add([pscustomobject]@{ProcessId=424242; PressureOwned=$false})' + "`n" + $needle)
        }
        else {
            $needle = 'Test-PressureProcessIdentity -ActualCreation $p.StartTime -ExpectedCreation ([datetime]$row.CreationDate)'
            Assert-PressureReceipt ($faultSource.Contains($needle)) 'sample identity fault site disappeared'
            $faultSource = $faultSource.Replace($needle,'$false')
        }
        [IO.File]::WriteAllText((Join-Path $caseRoot 'owner-fixture-probe.ps1'),$faultSource,$utf8)
    }
    switch ($case) {
        "unbound" { $inputs = @() }
        "input-drift" { $argv += @("-ChangeInput", $input) }
        "executable-drift" { $argv += @("-ChangeInput", $declaredExe) }
        "command-failure" { $argv += @("-ChildExitCode", "7") }
        "failure-and-drift" { $argv += @("-ChildExitCode", "7", "-ChangeInput", $input) }
        "pressure-limit" { $limitMB = 1; $argv += @("-ChildSleepMs", "6000") }
        "missing-input" { $inputs = @(Join-Path $caseRoot "absent.txt") }
        "wildcard-command" { $command = "powershell*" }
        "failure-and-capture-fault" { $argv += @('-ChildExitCode','7') }
        "long-binary-output" { $argv += '-LongBinaryOutput' }
        "oversized-stage" { $argv += '-OversizedStage' }
        "invalid-interval" { $intervalMs = 0 }
        "invalid-drain" { $drainMs = 0 }
    }
    $configPath = Join-Path $caseRoot "case.json"
    $isDefaultOutput = $case -in @('default-output-first','default-output-second')
    $config = [ordered]@{
        label = if ($isDefaultOutput) { 'default-repeat' } else { $case }
        command = $command
        argv = $argv
        inputs = $inputs
        executables = @($declaredExe)
        limit_mb = $limitMB
        interval_ms = $intervalMs
        drain_ms = $drainMs
        capture_fault = $captureFault
        unattributed_worker = $case -eq 'unattributed-worker'
        identity_sample_mismatch = $case -eq 'sample-identity-mismatch'
        default_outdir = $isDefaultOutput
        output = Join-Path $caseRoot "receipt"
    }
    $occupiedPath = $null
    if ($case -in @('occupied-stdout','occupied-summary')) {
        New-Item -ItemType Directory -Path $config.output | Out-Null
        $occupiedSuffix = if ($case -eq 'occupied-stdout') { 'stdout.log' } else { 'summary.json' }
        $occupiedPath = Join-Path $config.output "$case.$occupiedSuffix"
        [IO.File]::WriteAllText($occupiedPath,'preserved prior evidence',$utf8)
    }
    [System.IO.File]::WriteAllText($configPath, ($config | ConvertTo-Json -Depth 4), $utf8)
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $hostExe
    # -File preserves the script's exact exit code. -EncodedCommand maps a
    # failed nested script to 1, which would conceal the pressure owner's 89.
    $info.Arguments = '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' +
        $PSCommandPath + '" -CaseFile "' + $configPath + '"'
    $info.WorkingDirectory = if ($isDefaultOutput) { $defaultWorkingDirectory } else { $repoRoot }
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $worker = New-Object System.Diagnostics.Process
    $worker.StartInfo = $info
    [void]$worker.Start()
    $stdoutTask = $worker.StandardOutput.ReadToEndAsync()
    $stderrTask = $worker.StandardError.ReadToEndAsync()
    if (-not $worker.WaitForExit(45000)) {
        # Exact process object, never a shared process-name kill.
        $worker.Kill()
        throw "[build-pressure-receipt] $case exceeded its focused budget"
    }
    Assert-PressureReceipt ($stdoutTask.Wait(5000) -and $stderrTask.Wait(5000)) "$case output did not drain"
    $workerExit = $worker.ExitCode
    $workerOutput = $stdoutTask.Result + $stderrTask.Result
    $worker.Dispose()
    $summaryPath = Join-Path $config.output "$case.summary.json"
    if ($isDefaultOutput) {
        $reported = [regex]::Match($workerOutput, '(?m)^\[build-pressure\].* summary=(.+?)\r?$')
        Assert-PressureReceipt ($reported.Success) "$case did not report its selected evidence lifetime: $workerOutput"
        $summaryPath = [IO.Path]::GetFullPath((Join-Path $defaultWorkingDirectory $reported.Groups[1].Value))
        $defaultBoundary = [IO.Path]::GetFullPath((Join-Path $defaultWorkingDirectory '.tmp/build-pressure')) +
            [IO.Path]::DirectorySeparatorChar
        Assert-PressureReceipt ($summaryPath.StartsWith($defaultBoundary,[StringComparison]::OrdinalIgnoreCase)) `
            "$case escaped its default evidence boundary"
        $config.output = Split-Path -Parent $summaryPath
    }
    if ($case -in @('occupied-stdout','occupied-summary')) {
        Assert-PressureReceipt ($workerExit -ne 0 -and -not (Test-Path -LiteralPath $childReceiptPath) -and
            [IO.File]::ReadAllText($occupiedPath) -ceq 'preserved prior evidence' -and
            @(Get-ChildItem -LiteralPath $config.output -File).Count -eq 1 -and
            $workerOutput.Contains('pressure output already exists')) "$case overwrote evidence or launched its child"
        Write-Output "[build-pressure-receipt] PASS $case"
        continue
    }
    if ($case -in @('missing-input','wildcard-command','invalid-interval','invalid-drain')) {
        Assert-PressureReceipt ($workerExit -ne 0 -and -not (Test-Path -LiteralPath $summaryPath) -and
            -not (Test-Path -LiteralPath $childReceiptPath)) `
            "$case was admitted: $workerOutput"
        Write-Output "[build-pressure-receipt] PASS $case"
        continue
    }
    Assert-PressureReceipt (Test-Path -LiteralPath $summaryPath) "$case lost its receipt: $workerOutput"
    $receipt = Get-Content -LiteralPath $summaryPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $expectedExit = switch ($case) {
        "input-drift" { 89 }
        "executable-drift" { 89 }
        "command-failure" { 7 }
        "failure-and-drift" { 7 }
        "pressure-limit" { 88 }
        "capture-fault" { 90 }
        "failure-and-capture-fault" { 7 }
        "oversized-stage" { 90 }
        "unattributed-worker" { 90 }
        "sample-identity-mismatch" { 90 }
        default { 0 }
    }
    Assert-PressureReceipt ($workerExit -eq $expectedExit -and $receipt.exit_code -eq $expectedExit) `
        "$case exit/receipt mismatch: $workerOutput"
    Assert-PressureReceipt ($receipt.schema -eq "pgy.build-pressure.v3") "$case schema drifted"
    Assert-PressureReceipt ($receipt.execution.cwd -eq $info.WorkingDirectory -and
        $receipt.execution.command_path -eq $hostExe) "$case execution identity drifted"
    Assert-PressureReceipt (($receipt.execution.argv | ConvertTo-Json -Compress) -ceq
        ($argv | ConvertTo-Json -Compress)) "$case argv lost order, quoting or Unicode"
    if ($case -eq "pressure-limit") {
        Assert-PressureReceipt ($receipt.limit_exceeded -and $receipt.stop_on_limit -and
            -not $receipt.detached_compiler_worker_tracking -and $receipt.limit_mb -eq 1) `
            "the deliberately exceeded self-test ceiling did not refuse its own child"
        Write-Output "[build-pressure-receipt] PASS pressure-limit"
        continue
    }
    if ($case -in @('unattributed-worker','sample-identity-mismatch')) {
        Assert-PressureReceipt ($receipt.execution.command_exit_code -eq 0 -and $receipt.output_capture_complete -and
            $receipt.process_observation_scope -eq 'validated-root-tree') "$case concealed command/output results"
        if ($case -eq 'unattributed-worker') {
            Assert-PressureReceipt (-not $receipt.detached_worker_attribution_complete -and
                -not $receipt.process_identity_mismatch_seen) 'unowned candidate became a complete measurement'
        }
        else {
            Assert-PressureReceipt ($receipt.process_identity_mismatch_seen) 'changed sample identity became complete metrics'
        }
        Write-Output "[build-pressure-receipt] PASS $case"
        continue
    }
    Assert-PressureReceipt ($receipt.process_observation_scope -eq 'validated-root-tree' -and
        $receipt.detached_worker_attribution_complete -and
        -not $receipt.process_identity_mismatch_seen -and
        $receipt.execution.input_change_scope -eq 'before-after-only' -and
        $receipt.memory_peak_scope -eq 'observed-samples-only') "$case overclaimed identity, temporal binding or memory scope"
    if ($case -in @('capture-fault','failure-and-capture-fault','oversized-stage')) {
        $expectedCommandExit = if ($case -eq 'failure-and-capture-fault') { 7 } else { 0 }
        Assert-PressureReceipt (-not $receipt.output_capture_complete -and
            $receipt.output_capture_failure.Length -gt 0) "$case lost its explicit capture failure"
        if ($case -eq 'capture-fault') {
            Assert-PressureReceipt ($receipt.capture_abort_requested) 'live command was not aborted after a capture fault'
        }
        elseif ($case -eq 'failure-and-capture-fault') {
            Assert-PressureReceipt ($receipt.execution.command_exit_code -eq $expectedCommandExit) 'completed command failure was concealed by capture failure'
        }
        Write-Output "[build-pressure-receipt] PASS $case"
        continue
    }
    Assert-PressureReceipt ($receipt.output_capture_complete -and
        -not $receipt.detached_compiler_worker_tracking) "$case lost capture or adopted detached workers"
    Assert-PressureReceipt ($receipt.execution.heap_counter_state -eq "UNMEASURED" -and
        $receipt.execution.PSObject.Properties.Name -contains "heap_peak_live_bytes" -and
        $receipt.execution.PSObject.Properties.Name -contains "heap_total_allocated_bytes" -and
        $null -eq $receipt.execution.heap_peak_live_bytes -and
        $null -eq $receipt.execution.heap_total_allocated_bytes) "$case invented heap counters"
    $childLog = Get-Content -LiteralPath (Join-Path $config.output "$($config.label).stdout.log") -Raw -Encoding UTF8
    $marker = "RECEIPT_CHILD:" + [Convert]::ToBase64String($utf8.GetBytes($payload))
    Assert-PressureReceipt ($childLog.Contains($marker)) "$case changed the actual child argument"
    if ($case -eq 'long-binary-output') {
        $hasher = [Security.Cryptography.SHA256]::Create()
        try {
            $prefix = $utf8.GetBytes($marker + "`r`n[driver-pressure-stage]fixture:begin`r`n")
            [void]$hasher.TransformBlock($prefix,0,$prefix.Length,$null,0)
            $block = [Text.Encoding]::ASCII.GetBytes(('X' * 4096))
            for ($i = 0; $i -lt 1024; $i++) { [void]$hasher.TransformBlock($block,0,$block.Length,$null,0) }
            $binary = [byte[]]@(255,0,13,10)
            [void]$hasher.TransformBlock($binary,0,$binary.Length,$null,0)
            $tail = $utf8.GetBytes('[driver-pressure-stage]fixture:end')
            [void]$hasher.TransformFinalBlock($tail,0,$tail.Length)
            $expectedHash = ([BitConverter]::ToString($hasher.Hash)).Replace('-','').ToLowerInvariant()
        }
        finally { $hasher.Dispose() }
        $rawPath = Join-Path $config.output "$case.stdout.log"
        Assert-PressureReceipt ((Get-FileHash -LiteralPath $rawPath -Algorithm SHA256).Hash.ToLowerInvariant() -eq $expectedHash -and
            $receipt.max_pending_stage_characters -le 4096 -and $receipt.observed_stage_count -eq 3 -and
            (Get-Content -LiteralPath (Join-Path $config.output "$case.stages.csv") -Raw).Contains('fixture:end')) `
            'long/binary output changed bytes, retained an unbounded stage or lost trailing/stderr metadata'
    }
    $changed = @($receipt.execution.file_bindings | Where-Object { -not $_.unchanged })
    $shouldChange = $case -in @("input-drift", "executable-drift", "failure-and-drift")
    Assert-PressureReceipt ($receipt.execution.binding_changed -eq $shouldChange -and
        ($changed.Count -gt 0) -eq $shouldChange) "$case binding state incorrect"
    $boundInputs = @($receipt.execution.file_bindings | Where-Object { $_.role -eq "input" })
    if ($case -eq "unbound") {
        Assert-PressureReceipt ($receipt.execution.input_scope -eq "unbound" -and $boundInputs.Count -eq 0) `
            "no declared inputs became a baseline"
    }
    else {
        Assert-PressureReceipt ($receipt.execution.input_scope -eq "declared-paths-only" -and
            $boundInputs.Count -eq 2 -and $boundInputs[0].path -ceq $input -and
            $boundInputs[0].before.sha256 -eq $expectedInputHash) "$case lost input bytes/Unicode path"
    }
    $exeBinding = @($receipt.execution.file_bindings | Where-Object { $_.role -eq "executable" })
    Assert-PressureReceipt ($exeBinding.Count -eq 1 -and
        $exeBinding[0].before.sha256 -eq $expectedExeHash) "$case lost declared executable identity"
    $expectedCommandExit = if ($case -in @("command-failure", "failure-and-drift")) { 7 } else { 0 }
    Assert-PressureReceipt ($receipt.execution.command_exit_code -eq $expectedCommandExit) `
        "$case concealed the measured command's exit"
    if ($isDefaultOutput) {
        $directories = @(Get-ChildItem -LiteralPath (Join-Path $defaultWorkingDirectory '.tmp/build-pressure') -Directory)
        if ($case -eq 'default-output-first') {
            Assert-PressureReceipt ($directories.Count -eq 1) 'first default run did not own one fresh directory'
            $firstDefaultDirectory = $config.output
            $firstDefaultBindings = @(Get-ChildItem -LiteralPath $config.output -File | Get-FileHash -Algorithm SHA256)
            Assert-PressureReceipt ($firstDefaultBindings.Count -eq 5) 'first default run lost one of its five evidence files'
        }
        else {
            Assert-PressureReceipt ($directories.Count -eq 2 -and $config.output -cne $firstDefaultDirectory -and
                @(Get-ChildItem -LiteralPath $config.output -File).Count -eq 5) `
                'second default run reused an evidence lifetime or lost files'
            foreach ($binding in $firstDefaultBindings) {
                Assert-PressureReceipt ((Get-FileHash -LiteralPath $binding.Path -Algorithm SHA256).Hash -eq $binding.Hash) `
                    'repeated default invocation altered the first receipt bytes'
            }
        }
    }
    Write-Output "[build-pressure-receipt] PASS $case"
}

Write-Output "[build-pressure-receipt] PASS $($cases.Count) executable receipt cases plus synthetic identity controls; artifacts: $runRoot"
