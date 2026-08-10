param(
    [string]$PgyBin = "bin\pgy.exe",
    [string]$OutDir = "build\grammar_examples_compile",
    [int]$ExpectedCount = 17
)

$ErrorActionPreference = "Stop"
$RootDir = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $RootDir

if (-not (Test-Path -LiteralPath $PgyBin -PathType Leaf)) {
    Write-Error "[grammar-examples] missing executable compiler: $PgyBin"
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
    Write-Error "[grammar-examples] grammar example count drifted: $($sources.Count) != $ExpectedCount"
    exit 1
}

foreach ($src in $sources) {
    $rel = $src.FullName.Substring($RootDir.Path.Length + 1)
    $outName = ($rel -replace '[\\/:*?"<>|]', '_') + ".c"
    $outPath = Join-Path $OutDir $outName

    & $PgyBin "--native-pipeline" "--ast" $src.FullName > $null
    $rc = $LASTEXITCODE
    if ($rc -ne 0) {
        Write-Error "[grammar-examples] AST failed: $rel (exit=$rc)"
        exit $rc
    }

    & $PgyBin $src.FullName "--emit-c" "-o" $outPath > $null
    $rc = $LASTEXITCODE
    if ($rc -ne 0) {
        Write-Error "[grammar-examples] emit-c failed: $rel (exit=$rc)"
        exit $rc
    }
    if (-not (Test-Path -LiteralPath $outPath -PathType Leaf) -or
        (Get-Item -LiteralPath $outPath).Length -le 0) {
        Write-Error "[grammar-examples] empty emitted C for $rel"
        exit 1
    }
}

Write-Host "[grammar-examples] $ExpectedCount syntax examples parse and emit C"
