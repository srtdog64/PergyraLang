# AIR erasure measurement harness.
# For each fixture: emit pre-link LLVM IR (logical residue = named axis calls the
# codegen produces) and emit C -> compile at -O2 -> categorize the undefined
# symbols that SURVIVE inlining (physical residue = real machine dependencies).
#
# The two columns together are the erasure dashboard: codegen NAMES axis ops for
# traceability (logical > 0), but at -O2 the static-inline runtime folds them, so
# what physically survives is only the irreducible primitive the axis stood for
# (sync for concurrency, an abort path for a fail-closed guard, a load for a slot
# read). AxisCall physical == 0 means the axis VOCABULARY is fully compiled out.
#
# Usage:  pwsh tests/air_erasure/measure.ps1
# Requires: bin/pgy.exe, mingw gcc/nm/size on PATH, LLVM bin on PATH.
$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $root) { $root = (Get-Location).Path }
Set-Location $root

$AXIS = 'pgy_(channel|chan_|world|spawn|zone|intent|write|read|release|claim|slot|secure_|pin|unpin|device_|runtime_lifecycle)'
$cats = [ordered]@{
  'Axis'   = "^$AXIS"                              # axis runtime call surviving as out-of-line ref
  'Sync'   = '(pthread_|Fiber|ConvertThread)'      # concurrency primitives (bucket A)
  'Heap'   = '^(malloc|free|calloc|realloc)$'      # allocation
  'Abort'  = '(abort|__assert|_assert)'            # fail-closed path (bucket B/C)
  'IO'     = '^(printf|puts|fputs|fwrite|fflush|putchar)$'  # explicit Log/output
}
$fix = Get-ChildItem "tests/air_erasure/fixtures/*.pgy" | Sort-Object Name
$rows = @()
foreach ($f in $fix) {
  $b = $f.BaseName
  & ./bin/pgy.exe $f.FullName --emit-llvm -o ".tmp/$b.ll" 2>$null | Out-Null
  & ./bin/pgy.exe $f.FullName --emit-c   -o ".tmp/$b.c"  2>$null | Out-Null
  gcc -O2 -I src -I src/runtime -c ".tmp/$b.c" -o ".tmp/$b.o" 2>$null
  $ll = (Get-Content ".tmp/$b.ll" -Raw -ErrorAction SilentlyContinue); if (-not $ll) { $ll = '' }
  $logical = ([regex]::Matches($ll, "call[^\n]*@($AXIS)")).Count
  $u = & nm.exe -u ".tmp/$b.o" 2>$null | ForEach-Object { ($_ -split '\s+')[-1] }
  $text = ((& size.exe ".tmp/$b.o" 2>$null) | Select-Object -Last 1) -split '\s+' | Where-Object { $_ -match '^\d+$' } | Select-Object -First 1
  $o = [ordered]@{ Fixture = $b; AxisCallsEmitted = $logical; text = $text }
  foreach ($c in $cats.Keys) { $o[$c] = (($u | Where-Object { $_ -match $cats[$c] }) | Measure-Object).Count }
  $rows += [pscustomobject]$o
}
$rows | Format-Table -AutoSize
$rows | ConvertTo-Csv -NoTypeInformation | Set-Content "tests/air_erasure/results.csv"
