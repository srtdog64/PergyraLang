#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROBE="$ROOT_DIR/scripts/measure_build_pressure.ps1"
DRIVER_PARITY="$ROOT_DIR/tests/self_hosted/parity/driver_rung2_body_parity.sh"
DRIVER_MAIN="$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy"
DRIVER_BOOTSTRAP="$ROOT_DIR/tests/self_hosted/parity/driver_bootstrap.sh"

require() {
    grep -Fq -- "$1" "$PROBE" || {
        echo "[build-pressure-contract] missing: $1" >&2
        exit 1
    }
}

require 'schema = "pgy.build-pressure.v2"'
require 'compile_proc_count'
require 'link_proc_count'
require '$isCompiler -and $commandLine'
require '$phaseStats'
require 'peak_working_set_mb'
require 'peak_private_mb'
require 'ConvertTo-Json -Depth 4'
require '$summaryPath'
require '$stagePath'
require 'observed_elapsed_ms,stream,stage'
require '$invariantCulture = [Globalization.CultureInfo]::InvariantCulture'
require '$workingSet.ToString("F1", $invariantCulture)'
require '$private.ToString("F1", $invariantCulture)'
require '$topPrivate.ToString("F1", $invariantCulture)'
if grep -Fq '{5:N1},{6:N1}' "$PROBE"; then
    echo "[build-pressure-contract] locale-aware numeric formatting can corrupt sample CSV columns" >&2
    exit 1
fi
require 'BuildPressureOutputCapture'
require 'ReadAsync('
require '[driver-pressure-stage]'
require '[semantic-body-type-stage]'
require '[semantic-initializer-stage]'
require 'WaitForCompletion'
require 'AbortReaders'
require 'output_capture_complete = $outputCaptureComplete'
require '[int]$OutputDrainTimeoutMs = 5000'
if grep -Fq 'BeginOutputReadLine()' "$PROBE"; then
    echo "[build-pressure-contract] line-normalizing stdout capture returned" >&2
    exit 1
fi
require 'stages = $stagePath'
require '$env:MAKEFLAGS = $null'
require '$env:MAKELEVEL = $null'
require 'New-Object System.Diagnostics.ProcessStartInfo'
require 'EnvironmentVariables["PGY_BUILD_PRESSURE_ACTIVE"] = "1"'
require 'StartedAt $started'
require 'cc1|cc1plus|lto1|lto-wrapper|collect2|ld'
require 'pgy|pgy-self-driver|parser_ast_producer|gen[0-9]+'
require 'driver_(oracle|seed|gen[0-9]+|c|llvm)'
require '[switch]$RootProcessTreeOnly'
require '$trackDetachedCompilerWorkers = -not [bool]$RootProcessTreeOnly'
require 'if ($IncludeDetachedCompilerWorkers) {'
require '-IncludeDetachedCompilerWorkers $trackDetachedCompilerWorkers'
require 'detached_compiler_worker_tracking = $trackDetachedCompilerWorkers'
require '$ownedProcessIds = @($rows | ForEach-Object { [int]$_.ProcessId })'
require 'foreach ($id in ($ownedProcessIds | Sort-Object -Descending))'
if grep -Fq 'detached_compiler_worker_tracking = $true' "$PROBE" ||
   grep -Fq 'Stop-Process -Id $toolPid' "$PROBE"; then
    echo "[build-pressure-contract] root-only mode can still report or stop an unowned detached worker" >&2
    exit 1
fi
require '$exitCode = if ($rootExitComplete) { [int]$process.ExitCode } else { -1 }'
require 'New-Object System.Text.UTF8Encoding($false)'
require '[switch]$StopOnLimit'
require 'limit_exceeded = $limitExceeded'
require 'if ($StopOnLimit)'

grep -Fq 'PGY_BUILD_PRESSURE_LIMIT_MB ?= 3072' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] missing 3 GiB build ceiling" >&2; exit 1; }
grep -Fq 'build-pressure-dev-compiler:' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] missing dev compiler probe" >&2; exit 1; }
grep -Fq 'build-pressure-compiler:' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] missing full compiler probe" >&2; exit 1; }
grep -Fq 'build-pressure-self-host-compiler:' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] missing self-host compiler probe" >&2; exit 1; }
if [[ "$(grep -c -- '-StopOnLimit' "$ROOT_DIR/Makefile")" -lt 4 ]]; then
    echo "[build-pressure-contract] compiler probes do not enforce the hard memory ceiling" >&2
    exit 1
fi
grep -Fq -- '-Label self-host-driver-fixpoint' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] full driver fixpoint is outside the pressure probe" >&2; exit 1; }
grep -Fq 'elif [ "$${OS:-}" = "Windows_NT" ]; then' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] Windows full fixpoint can bypass the pressure owner" >&2; exit 1; }
grep -Fq 'Windows full fixpoint requires the PowerShell pressure owner' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] Windows pressure-owner diagnostic drifted" >&2; exit 1; }
grep -Fq 'self-host-driver-bootstrap-full-pressure-body-test-smoke' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] full driver fixpoint lacks a bounded body target" >&2; exit 1; }
grep -Fq 'full pressure body requires measure_build_pressure.ps1' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] full driver body can bypass its pressure owner" >&2; exit 1; }
grep -Fq 'full driver MIR production requires the pressure-owned bootstrap gate' "$DRIVER_MAIN" \
    || { echo "[build-pressure-contract] full driver binary lacks its direct-call rejection" >&2; exit 1; }
grep -Fq 'args[3] != "--pressure-owned-full-fixpoint"' "$DRIVER_MAIN" \
    || { echo "[build-pressure-contract] full driver binary pressure token drifted" >&2; exit 1; }
grep -Fq 'CompileSourceToMirJsonFilePressureObserved(' "$DRIVER_MAIN" \
    || { echo "[build-pressure-contract] full driver binary lacks stage observation" >&2; exit 1; }
grep -Fq '[driver-pressure-stage]' "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy" \
    || { echo "[build-pressure-contract] full driver pressure stages are not observable" >&2; exit 1; }
grep -Fq '[semantic-body-type-stage]' "$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy" \
    || { echo "[build-pressure-contract] body type pressure stages are not observable" >&2; exit 1; }
grep -Fq '[semantic-initializer-stage]' "$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy" \
    || { echo "[build-pressure-contract] initializer pressure stages are not observable" >&2; exit 1; }
grep -Fq '"--pressure-owned-full-fixpoint"' "$DRIVER_BOOTSTRAP" \
    || { echo "[build-pressure-contract] full driver runner lacks the pressure-owned token" >&2; exit 1; }
grep -Fq '*"/Git/"*"/bash.exe")' "$DRIVER_PARITY" \
    || { echo "[build-pressure-contract] full DRV-2 matrix lacks the Git Bash orphan guard" >&2; exit 1; }
grep -Fq 'full matrix requires MSYS2 bash' "$DRIVER_PARITY" \
    || { echo "[build-pressure-contract] full DRV-2 matrix shell diagnostic drifted" >&2; exit 1; }
grep -Fq 'PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER for a focused Git Bash gate' "$DRIVER_PARITY" \
    || { echo "[build-pressure-contract] focused Git Bash escape hatch is undocumented" >&2; exit 1; }

echo "[build-pressure-contract] native/self-host hard ceiling and full-matrix shell guard wired"
