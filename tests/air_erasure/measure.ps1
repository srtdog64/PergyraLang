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
$pgy = Join-Path $repo 'bin\pgy.exe'

$AXIS = 'pgy_(channel|chan_|world|spawn|zone|intent|write|read|release|claim|slot|secure_|pin|unpin|device_|runtime_lifecycle)'
$catsPhys = [ordered]@{
  'Axis'  = "^$AXIS"
  'Sync'  = '(pthread_|Fiber|ConvertThread)'
  'Abort' = '(abort|__assert|_assert)'
}
if (-not (Test-Path ".tmp")) { New-Item -ItemType Directory ".tmp" | Out-Null }
$fix = Get-ChildItem "tests\air_erasure\fixtures\*.pgy" | Sort-Object Name
$rows = @()
foreach ($f in $fix) {
  $name = [string]$f.BaseName
  # --- AIR declared (A/B/C) ---
  $air = (& $pgy $f.FullName --air-json 2>$null | Out-String)
  if (-not $air) { $air = '' }
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
  & $pgy $f.FullName --emit-c -o ".tmp\$name.c" 2>$null | Out-Null
  & gcc -O2 -I src -I src\runtime -c ".tmp\$name.c" -o ".tmp\$name.o" 2>$null
  $u = & nm.exe -u ".tmp\$name.o" 2>$null | ForEach-Object { ($_ -split '\s+')[-1] }
  $o = [ordered]@{ Fixture = $name; 'A_inh' = $A; 'B_pol' = $B; 'C_unprov' = $C }
  foreach ($c in $catsPhys.Keys) { $o["phys_$c"] = (($u | Where-Object { $_ -match $catsPhys[$c] }) | Measure-Object).Count }
  $rows += [pscustomobject]$o
}
$rows | Format-Table -AutoSize
$rows | ConvertTo-Csv -NoTypeInformation | Set-Content "tests\air_erasure\results.csv"
Write-Output "saved tests\air_erasure\results.csv"
