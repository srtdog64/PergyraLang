# Capability manifest + declared>=used gate (PowerShell harness).
#
# Capability is a first-class, interprocedurally-inferred refinement of effects.
# `pgy --capability-manifest` prints the inferred capability set; the per-function
# `with caps` declared>=used check is enforced during semantic analysis.
#
#   manifest_clean / manifest_declared_ok -> exit 0
#   manifest_violation                    -> exit 1 (GetTimestamp: missing clock)
#   manifest_interproc                    -> exit 1 (entry: clock via helper())
#   print_violation                       -> exit 1 (EmitProtocol: missing io_write)

# Continue (not Stop): pgy writes its diagnostic summary to stderr, and under
# Stop + 2>&1 PowerShell 5.1 promotes native stderr to a terminating error.
$ErrorActionPreference = "Continue"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $root
[Environment]::CurrentDirectory = $root

$pgy = Join-Path $root "bin\pgy.exe"
if (-not (Test-Path $pgy)) { Write-Host "[SKIP] bin/pgy.exe not built"; exit 0 }
$llvm = "C:\Program Files\LLVM\bin"
if (Test-Path $llvm) { $env:Path = "$llvm;" + $env:Path }

$fail = 0

function Run-Manifest($file) {
    & $pgy --capability-manifest $file 2>&1 | Out-String
}

# --- expect-clean ---
foreach ($f in @("manifest_clean", "manifest_declared_ok")) {
    $out = Run-Manifest "tests\capability\$f.pgy"
    if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] $f exit=$LASTEXITCODE (want 0)"; $fail++ }
    elseif ($out -match "missing declared capabilities") { Write-Host "[FAIL] $f reported a violation"; $fail++ }
    else { Write-Host "[PASS] $f clean (exit 0)" }
}
$cleanOut = Run-Manifest "tests\capability\manifest_clean.pgy"
foreach ($cap in @("IO_WRITE", "CLOCK", "RANDOM")) {
    if ($cleanOut -notmatch $cap) { Write-Host "[FAIL] clean manifest missing $cap"; $fail++ }
}

# --- expect-violation ---
function Check-Violation($file, $fn) {
    $out = Run-Manifest "tests\capability\$file.pgy"
    $rc = $LASTEXITCODE
    if ($rc -eq 0) { Write-Host "[FAIL] $file exit=0 (gate did not fire)"; $script:fail++; return }
    if ($out -notmatch "missing declared capabilities") { Write-Host "[FAIL] $file missing violation message"; $script:fail++ }
    if ($out -notmatch $fn) { Write-Host "[FAIL] $file should name '$fn'"; $script:fail++ }
    if ($out -notmatch "clock") { Write-Host "[FAIL] $file should name missing 'clock'"; $script:fail++ }
    if ($rc -ne 0 -and $out -match "missing declared capabilities" -and $out -match $fn) {
        Write-Host "[PASS] $file under-declaration gate fired ($fn -> clock, exit $rc)"
    }
}
Check-Violation "manifest_violation" "GetTimestamp"
Check-Violation "manifest_interproc" "entry"

function Check-IOViolation($file, $fn, $missingCap) {
    $out = Run-Manifest "tests\capability\$file.pgy"
    $rc = $LASTEXITCODE
    if ($rc -eq 0) { Write-Host "[FAIL] $file exit=0 (gate did not fire)"; $script:fail++; return }
    if ($out -notmatch "missing declared capabilities") { Write-Host "[FAIL] $file missing violation message"; $script:fail++ }
    if ($out -notmatch $fn) { Write-Host "[FAIL] $file should name '$fn'"; $script:fail++ }
    if ($out -notmatch $missingCap) { Write-Host "[FAIL] $file should name missing '$missingCap'"; $script:fail++ }
    if ($out -match "missing declared capabilities" -and $out -match $fn -and $out -match $missingCap) {
        Write-Host "[PASS] $file under-declaration gate fired ($fn -> $missingCap, exit $rc)"
    }
}
Check-IOViolation "file_handle_write_violation" "StreamWrite" "io_write"
Check-IOViolation "file_handle_read_violation" "StreamRead" "io_read"
Check-IOViolation "file_handle_dynamic_mode_violation" "OpenDynamic" "io_write"
Check-IOViolation "file_handle_read_write_violation" "OpenReadWrite" "io_read"
Check-IOViolation "file_exists_violation" "ProbeExists" "io_read"
Check-IOViolation "print_violation" "EmitProtocol" "io_write"

if ($fail -eq 0) { Write-Host "ALL PASS (0 failures)"; exit 0 }
Write-Host "FAILED ($fail)"; exit 1
