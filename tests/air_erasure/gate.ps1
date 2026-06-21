# AIR erasure CI gate. Consumes tests/air_erasure/results.csv (produced by
# measure.ps1) plus baseline.json, and enforces three contracts:
#
#   1. Erasure contract (hard): every provable fixture must compile to ZERO
#      surviving axis calls AND ZERO fail-closed abort paths. A regression here
#      means a statically-provable program started paying runtime cost.
#   2. Bucket-C monotonicity (hard): the total unproven-retain count (lifecycle
#      CHECK guards the analysis could not erase) must NOT exceed the committed
#      baseline. C may only shrink; growth means the static analysis weakened.
#   3. Declared-vs-measured drift (report): a fixture that AIR declares fully
#      compressed (A+B+C == 0) yet shows physical residue is an
#      AIR_DRIFT_COMPRESSION_RESIDUE_MISMATCH — AIR's model under-covers it.
#      Reported, not failed (it flags a modeling gap, not a correctness break).
#
# Exit non-zero on any hard-contract violation. Run measure.ps1 first.
$ErrorActionPreference = 'Stop'
$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
$csv = Join-Path $dir 'results.csv'
$base = Get-Content (Join-Path $dir 'baseline.json') -Raw | ConvertFrom-Json
if (-not (Test-Path $csv)) { Write-Error "[air-gate] results.csv missing; run measure.ps1 first"; exit 2 }
$rows = Import-Csv $csv
$fail = 0

# 1. erasure contract
foreach ($name in $base.provable_fixtures_must_be_clean) {
  $r = $rows | Where-Object { $_.Fixture -eq $name }
  if (-not $r) { Write-Output "[air-gate] FAIL erasure: fixture '$name' not measured"; $fail++; continue }
  if ([int]$r.phys_Axis -ne 0 -or [int]$r.phys_Abort -ne 0) {
    Write-Output "[air-gate] FAIL erasure: '$name' has phys_Axis=$($r.phys_Axis) phys_Abort=$($r.phys_Abort) (must be 0/0)"
    $fail++
  } else {
    Write-Output "[air-gate] ok erasure: '$name' clean (Axis=0 Abort=0)"
  }
}

# 2. bucket-C monotonicity
$cTotal = ($rows | Measure-Object -Property C_unprov -Sum).Sum
if ($cTotal -gt [int]$base.total_unproven_retain) {
  Write-Output "[air-gate] FAIL bucket-C: total unproven=$cTotal exceeds baseline=$($base.total_unproven_retain) (analysis regressed)"
  $fail++
} else {
  Write-Output "[air-gate] ok bucket-C: total unproven=$cTotal <= baseline=$($base.total_unproven_retain)"
}

# 3. drift report (non-fatal)
foreach ($r in $rows) {
  $declared = [int]$r.A_inh + [int]$r.B_pol + [int]$r.C_unprov
  $phys = [int]$r.phys_Axis + [int]$r.phys_Sync + [int]$r.phys_Abort
  if ($declared -eq 0 -and $phys -gt 0) {
    $expected = $base.expected_drifts.PSObject.Properties.Name -contains $r.Fixture
    if ($expected) {
      Write-Output "[air-gate] drift (expected): '$($r.Fixture)' declared 0, physical=$phys (documented modeling gap)"
    } else {
      Write-Output "[air-gate] FAIL drift (NEW): '$($r.Fixture)' declared 0 but physical residue=$phys — unexpected compression_residue_mismatch, investigate"
      $fail++
    }
  }
}

if ($fail -gt 0) { Write-Output "[air-gate] FAILED ($fail hard violations)"; exit 1 }
Write-Output "[air-gate] PASSED"
exit 0
