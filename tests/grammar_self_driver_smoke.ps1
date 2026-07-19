param(
    [string]$DriverBin = "bin\pgy-self-driver.exe",
    [string]$OutDir = "build\grammar_self_driver",
    [int]$ExpectedCount = 17
)

$ErrorActionPreference = "Stop"
$RootDir = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $RootDir

if (-not (Test-Path -LiteralPath $DriverBin -PathType Leaf)) {
    Write-Error "[grammar-self-driver] missing self-host driver: $DriverBin"
    exit 1
}

$runtimePathCandidates = @(
    "C:\ProgramData\mingw64\mingw64\bin",
    "C:\msys64\mingw64\bin",
    "C:\msys64\ucrt64\bin",
    "C:\msys64\clang64\bin",
    "C:\LLVM\bin",
    "C:\Program Files\LLVM\bin"
)
$runtimePaths = @()
foreach ($candidate in $runtimePathCandidates) {
    if (Test-Path -LiteralPath $candidate -PathType Container) {
        $runtimePaths += $candidate
    }
}
if ($runtimePaths.Count -gt 0) {
    $env:Path = ($runtimePaths -join ";") + ";" + $env:Path
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$sources = @(Get-ChildItem -Path "grammar" -Recurse -Filter "*.pgy" | Sort-Object FullName)
if ($sources.Count -ne $ExpectedCount) {
    Write-Error "[grammar-self-driver] fixture count drifted: $($sources.Count) != $ExpectedCount"
    exit 1
}

foreach ($src in $sources) {
    # DRV-2 IO is intentionally repository-relative.  Keeping this path
    # relative prevents an absolute-path fallback from becoming an accidental
    # second source-of-truth policy.
    $rel = $src.FullName.Substring($RootDir.Path.Length + 1)
    $outName = ($rel -replace '[\\/:*?"<>|]', '_') + ".c"
    $outPath = Join-Path $OutDir $outName
    & $DriverBin $rel "--emit-c-verified" > $outPath
    $rc = $LASTEXITCODE
    if ($rc -ne 0) {
        Write-Error "[grammar-self-driver] DRV-2 failed: $rel (exit=$rc)"
        exit $rc
    }
    if (-not (Test-Path -LiteralPath $outPath -PathType Leaf) -or
        (Get-Item -LiteralPath $outPath).Length -le 0) {
        Write-Error "[grammar-self-driver] empty verified C: $rel"
        exit 1
    }
    $c = Get-Content -LiteralPath $outPath -Raw
    if ($c -notmatch '#include' -or $c -notmatch 'int main\(') {
        Write-Error "[grammar-self-driver] verified C surface incomplete: $rel"
        exit 1
    }
}

Write-Host "[grammar-self-driver] $ExpectedCount grammar fixtures pass DRV-2 verified C"
