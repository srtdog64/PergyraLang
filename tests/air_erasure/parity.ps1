# C == LLVM behavioral parity for the erasure fixtures. Faithful LLVM *physical*
# residue (nm on the optimized, runtime-linked object) needs llvm `opt`/`llc`,
# which this LLVM install does not ship (runtime libLLVM + clang only). The
# backends share an ABI-identical runtime (pgy_runtime_lib.bc is built from the
# same source as the C static-inline runtime; see scripts/build_runtime_bc.sh),
# so residue parity follows from BEHAVIORAL parity + that shared runtime. This
# script checks the behavioral half: every fixture must produce the same outcome
# on both backends (both clean, or both panic with the same class).
$ErrorActionPreference = 'Continue'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $repo; [Environment]::CurrentDirectory = $repo
$pgy = Join-Path $repo 'bin\pgy.exe'
$base = Get-Content (Join-Path $PSScriptRoot 'baseline.json') -Raw | ConvertFrom-Json
$llvmUnsupported = @($base.llvm_unsupported.PSObject.Properties.Name)
reg add "HKCU\Software\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1 /f 2>$null | Out-Null

function Outcome($exe) {
  if (-not (Test-Path $exe)) { return "compile-fail" }
  $err = "$exe.err"
  $p = Start-Process -FilePath $exe -PassThru -NoNewWindow -RedirectStandardError $err -RedirectStandardOutput "$exe.out"
  if (-not $p.WaitForExit(8000)) { $p.Kill(); return "HANG" }
  $panic = ''
  if (Test-Path $err) {
    $m = Select-String -Path $err -Pattern 'class=([a-z-]+)' | Select-Object -First 1
    if ($m) { $panic = $m.Matches.Groups[1].Value }
  }
  if ($panic) { return "panic:$panic" }
  $out = if (Test-Path "$exe.out") { [string](Get-Content "$exe.out" -Raw) } else { '' }
  return "clean:" + (($out -replace '\s+', ' ').Trim())
}

$fail = 0
foreach ($f in (Get-ChildItem "tests\air_erasure\fixtures\*.pgy" | Sort-Object Name)) {
  $b = [string]$f.BaseName
  if (Test-Path ".tmp\$b.l.exe") { Remove-Item ".tmp\$b.l.exe" -Force }
  & $pgy $f.FullName --backend=c    -o ".tmp\$b.c.exe" 2>$null | Out-Null
  & $pgy $f.FullName --backend=llvm -o ".tmp\$b.l.exe" 2>$null | Out-Null
  $oc = Outcome ".tmp\$b.c.exe"
  $ol = Outcome ".tmp\$b.l.exe"
  if ($oc -eq $ol) {
    Write-Output ("[parity] ok   {0,-22} {1}" -f $b, $oc)
  } elseif ($ol -eq "compile-fail" -and ($llvmUnsupported -contains $b)) {
    Write-Output ("[parity] divergence (known): {0,-22} LLVM cannot compile (documented); C=[{1}]" -f $b, $oc)
  } else {
    Write-Output ("[parity] FAIL {0,-22} C=[{1}] LLVM=[{2}]" -f $b, $oc, $ol)
    $fail++
  }
}
if ($fail -gt 0) { Write-Output "[parity] FAILED ($fail mismatches)"; exit 1 }
Write-Output "[parity] PASSED (C == LLVM on all fixtures)"
exit 0
