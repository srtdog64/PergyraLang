# AIR erasure measurement harness — joins AIR's DECLARED disposition with the
# MEASURED physical residue, per fixture.
#
# Two independent instruments, joined:
#   - AIR (declared): the compiler's own A/B/C decomposition of each retain,
#     read from `--air-json`:
#       A_inh    = boundaries with retain_cause "inherent" (runtime fact, bucket A)
#       B_pol    = boundaries with retain_cause "policy" plus program-wide
#                  slot_capability_retain_count (kept-by-policy, B)
#       C_unprov = unproven_retain_count (lifecycle CHECK guards the static
#                  analysis could not erase — the improvable bucket C)
#   - Physical (measured): what survives `gcc -O2` of the emitted C, via `nm -u`:
#       phys_Axis = out-of-line axis call, phys_Sync = pthread, phys_Abort = abort.
#
# The join is the point: AIR *claims* (A/B/C), the binary *shows* (physical).
# A drift check (step ③) flags when AIR says erase/none but a call survives, or
# vice-versa. The independence is what makes the numbers credible — a compiler
# grading its own erasure is not evidence.
#
# Usage:  pwsh tests/air_erasure/measure.ps1
# Requires: bin/pgy.exe, mingw gcc/nm on PATH, LLVM bin on PATH.
$ErrorActionPreference = 'Continue'

# Self-locate the repo root (this script lives at <repo>/tests/air_erasure/).
# PowerShell's cd does not set the process working directory, so a child
# `powershell -File` would otherwise fail to resolve .\bin\pgy.exe.
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $repo
[Environment]::CurrentDirectory = $repo
# PGY_BIN override lets CI point at its own BIN_DIR build instead of bin\pgy.exe.
$pgy = if ($env:PGY_BIN) { $env:PGY_BIN } else { Join-Path $repo 'bin\pgy.exe' }
$pgy = ([string]$pgy).Trim('"')
if (-not (Test-Path $pgy)) {
  Write-Output "[air-measure] FAIL pgy not found: $pgy"
  exit 1
}
$pgyDir = Split-Path -Parent (Resolve-Path $pgy)
$gccCmd = Get-Command gcc.exe -ErrorAction SilentlyContinue
$pathPrefix = @($pgyDir)
if ($gccCmd) { $pathPrefix += (Split-Path -Parent $gccCmd.Source) }
$env:PATH = (($pathPrefix | Select-Object -Unique) -join ';') + ';' + $env:PATH

$AXIS = 'pgy_(channel|chan_|world|spawn|zone|intent|write|read|release|claim|slot|secure_|pin|unpin|device_|runtime_lifecycle)'
$catsPhys = [ordered]@{
  'Axis'  = "^$AXIS"
  'Sync'  = '(pthread_|Fiber|ConvertThread)'
  'Abort' = '(abort|__assert|_assert)'
}
if (-not (Test-Path ".tmp")) { New-Item -ItemType Directory ".tmp" | Out-Null }
$fix = Get-ChildItem "tests\air_erasure\fixtures\*.pgy" | Sort-Object Name
$rows = @()
$fail = 0
foreach ($f in $fix) {
  $name = [string]$f.BaseName
  # --- AIR declared (A/B/C) ---
  $airErr = ".tmp\$name.air.err"
  $airOut = ".tmp\$name.air.json"
  Remove-Item -LiteralPath $airOut -ErrorAction SilentlyContinue
  & $pgy $f.FullName --air-json > $airOut 2>$airErr
  $airExit = $LASTEXITCODE
  $air = if (Test-Path $airOut) { Get-Content $airOut -Raw } else { '' }
  if ($airExit -ne 0 -or -not $air) {
    $airLen = if (Test-Path $airOut) { (Get-Item $airOut).Length } else { 0 }
    Write-Output "[air-measure] FAIL air-json: $name exit=$airExit out_len=$airLen pgy=$pgy"
    if (Test-Path $airErr) { Get-Content $airErr | ForEach-Object { Write-Output "  $_" } }
    $fail++
    continue
  }
  # bucket A = inherent boundaries + program-wide inherent concurrency retains
  $Abnd = ([regex]::Matches($air, 'retain_cause":"inherent"')).Count
  $mConc = [regex]::Match($air, 'inherent_concurrency_count":(\d+)')
  $Aconc = if ($mConc.Success) { [int]$mConc.Groups[1].Value } else { 0 }
  $A = $Abnd + $Aconc
  $Bbnd = ([regex]::Matches($air, 'retain_cause":"policy"')).Count
  $mSlotCap = [regex]::Match($air, 'slot_capability_retain_count":(\d+)')
  $Bslot = if ($mSlotCap.Success) { [int]$mSlotCap.Groups[1].Value } else { 0 }
  $B = $Bbnd + $Bslot
  $mC = [regex]::Match($air, 'unproven_retain_count":(\d+)')
  $C = if ($mC.Success) { [int]$mC.Groups[1].Value } else { 0 }
  # --- physical measured ---
  $cPath = ".tmp\$name.c"
  $oPath = ".tmp\$name.o"
  $emitErr = ".tmp\$name.emit.err"
  $gccErr = ".tmp\$name.gcc.err"
  $nmErr = ".tmp\$name.nm.err"
  Remove-Item -LiteralPath $cPath, $oPath -ErrorAction SilentlyContinue
  & $pgy $f.FullName --emit-c -o $cPath 2>$emitErr | Out-Null
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path $cPath)) {
    Write-Output "[air-measure] FAIL emit-c: $name"
    if (Test-Path $emitErr) { Get-Content $emitErr | ForEach-Object { Write-Output "  $_" } }
    $fail++
    continue
  }
  & gcc -std=c11 -O2 -I src -I src\runtime -c $cPath -o $oPath 2>$gccErr
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path $oPath)) {
    Write-Output "[air-measure] FAIL gcc: $name"
    if (Test-Path $gccErr) { Get-Content $gccErr | ForEach-Object { Write-Output "  $_" } }
    $fail++
    continue
  }
  $nmOut = & nm.exe -u $oPath 2>$nmErr
  if ($LASTEXITCODE -ne 0) {
    Write-Output "[air-measure] FAIL nm: $name"
    if (Test-Path $nmErr) { Get-Content $nmErr | ForEach-Object { Write-Output "  $_" } }
    $fail++
    continue
  }
  $u = $nmOut | ForEach-Object { ($_ -split '\s+')[-1] }
  $o = [ordered]@{ Fixture = $name; 'A_inh' = $A; 'B_pol' = $B; 'C_unprov' = $C }
  foreach ($c in $catsPhys.Keys) { $o["phys_$c"] = (($u | Where-Object { $_ -match $catsPhys[$c] }) | Measure-Object).Count }
  $rows += [pscustomobject]$o
  # The control fixture defines the runtime substrate floor: the named Sync/Abort
  # symbols every emitted program carries regardless of axis use (e.g. the R6
  # wall-time watchdog). gate.ps1 pins this list against baseline.json.
  if ($name -eq '00_pure_value') {
    $floorSyms = @($u | Where-Object { $_ -match $catsPhys['Sync'] -or $_ -match $catsPhys['Abort'] }) | Sort-Object -Unique
    $floorSyms | Set-Content ".tmp\air_erasure_floor_symbols.txt"
  }
}
$rows | Format-Table -AutoSize
$rows | ConvertTo-Csv -NoTypeInformation | Set-Content "tests\air_erasure\results.csv"
Write-Output "saved tests\air_erasure\results.csv"
if ($fail -gt 0) {
  Write-Output "[air-measure] FAILED ($fail fixture measurement errors)"
  exit 1
}
exit 0
