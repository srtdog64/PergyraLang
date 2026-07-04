# AIR erasure CI gate. Consumes tests/air_erasure/results.csv (produced by
# measure.ps1) plus baseline.json, and enforces four contracts:
#
#   0. Substrate-floor pin (hard): every emitted program carries a runtime
#      substrate (today: the R6 wall-time watchdog = pthread_create/detach +
#      its fail-close abort). The control fixture (00_pure_value) must match
#      baseline.substrate_floor EXACTLY -- both the counts and the NAMED symbol
#      list. Floor growth is RED until a human commits a new baseline with
#      attribution; this keeps "bounded, measured, attributed" honest.
#   1. Erasure contract (hard): every provable fixture must compile to ZERO
#      surviving axis calls AND nothing beyond the substrate floor. A
#      regression here means a statically-provable program started paying
#      per-axis runtime cost.
#   2. Bucket-C monotonicity (hard): the total unproven-retain count (lifecycle
#      CHECK guards the analysis could not erase) must NOT exceed the committed
#      baseline. C may only shrink; growth means the static analysis weakened.
#   3. Declared-vs-measured drift: a fixture that AIR declares fully
#      compressed (A+B+C == 0) yet shows physical residue BEYOND the floor is
#      an AIR_DRIFT_COMPRESSION_RESIDUE_MISMATCH -- hard-failed if new,
#      reported if listed in expected_drifts (a documented modeling gap).
#
# Exit non-zero on any hard-contract violation. Run measure.ps1 first.
$ErrorActionPreference = 'Stop'
$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = (Resolve-Path (Join-Path $dir '..\..')).Path
$csv = Join-Path $dir 'results.csv'
$base = Get-Content (Join-Path $dir 'baseline.json') -Raw | ConvertFrom-Json
if (-not (Test-Path $csv)) { Write-Error "[air-gate] results.csv missing; run measure.ps1 first"; exit 2 }
$rows = Import-Csv $csv
$fail = 0

$floorSync = [int]$base.substrate_floor.Sync
$floorAbort = [int]$base.substrate_floor.Abort

# 0. substrate-floor pin (control fixture + named symbol list)
$ctl = $rows | Where-Object { $_.Fixture -eq '00_pure_value' }
if (-not $ctl) {
  Write-Output "[air-gate] FAIL floor: control fixture 00_pure_value not measured"; $fail++
} elseif ([int]$ctl.phys_Axis -ne 0 -or [int]$ctl.phys_Sync -ne $floorSync -or [int]$ctl.phys_Abort -ne $floorAbort) {
  Write-Output "[air-gate] FAIL floor: control has Axis=$($ctl.phys_Axis) Sync=$($ctl.phys_Sync) Abort=$($ctl.phys_Abort), baseline floor is Axis=0 Sync=$floorSync Abort=$floorAbort -- substrate changed; attribute it and commit a new baseline"
  $fail++
} else {
  Write-Output "[air-gate] ok floor: control matches substrate floor (Sync=$floorSync Abort=$floorAbort)"
}
$floorFile = Join-Path $repo '.tmp\air_erasure_floor_symbols.txt'
if (-not (Test-Path $floorFile)) {
  Write-Output "[air-gate] FAIL floor: $floorFile missing; run measure.ps1 first (symbol-level floor attribution required)"
  $fail++
} else {
  $measuredSyms = @(Get-Content $floorFile | Where-Object { $_ } | Sort-Object -Unique)
  $baseSyms = @($base.substrate_floor.symbols | Sort-Object -Unique)
  $delta = Compare-Object $measuredSyms $baseSyms
  if ($delta) {
    Write-Output "[air-gate] FAIL floor: substrate symbol set drifted from baseline:"
    $delta | ForEach-Object { Write-Output "    $($_.SideIndicator) $($_.InputObject)" }
    $fail++
  } else {
    Write-Output "[air-gate] ok floor: substrate symbols pinned [$($baseSyms -join ', ')]"
  }
}

# 1. erasure contract (nothing beyond the floor for provable fixtures)
foreach ($name in $base.provable_fixtures_must_be_clean) {
  $r = $rows | Where-Object { $_.Fixture -eq $name }
  if (-not $r) { Write-Output "[air-gate] FAIL erasure: fixture '$name' not measured"; $fail++; continue }
  if ([int]$r.phys_Axis -ne 0 -or [int]$r.phys_Sync -ne $floorSync -or [int]$r.phys_Abort -ne $floorAbort) {
    Write-Output "[air-gate] FAIL erasure: '$name' has phys_Axis=$($r.phys_Axis) phys_Sync=$($r.phys_Sync) phys_Abort=$($r.phys_Abort) (must be Axis=0 and exactly the floor Sync=$floorSync Abort=$floorAbort)"
    $fail++
  } else {
    Write-Output "[air-gate] ok erasure: '$name' clean (Axis=0, nothing beyond floor)"
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

# 3. drift check on the floor-excess physical residue
foreach ($r in $rows) {
  $declared = [int]$r.A_inh + [int]$r.B_pol + [int]$r.C_unprov
  $excess = [int]$r.phys_Axis + ([int]$r.phys_Sync - $floorSync) + ([int]$r.phys_Abort - $floorAbort)
  if ($excess -lt 0) {
    Write-Output "[air-gate] note: '$($r.Fixture)' is BELOW the substrate floor ($excess) -- floor is not uniform, worth a look"
    continue
  }
  if ($declared -eq 0 -and $excess -gt 0) {
    $expected = $base.expected_drifts.PSObject.Properties.Name -contains $r.Fixture
    if ($expected) {
      Write-Output "[air-gate] drift (expected): '$($r.Fixture)' declared 0, physical excess=$excess (documented modeling gap)"
    } else {
      Write-Output "[air-gate] FAIL drift (NEW): '$($r.Fixture)' declared 0 but physical excess over floor=$excess -- unexpected compression_residue_mismatch, investigate"
      $fail++
    }
  }
}

if ($fail -gt 0) { Write-Output "[air-gate] FAILED ($fail hard violations)"; exit 1 }
Write-Output "[air-gate] PASSED"
exit 0
