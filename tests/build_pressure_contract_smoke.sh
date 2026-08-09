#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROBE="$ROOT_DIR/scripts/measure_build_pressure.ps1"
DRIVER_PARITY="$ROOT_DIR/tests/self_hosted/parity/driver_rung2_body_parity.sh"
DRIVER_MAIN="$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy"
# bcb1d8fa moved argv admission out of the bootstrap main into this owner, and
# turned the guard from "reject at argv[3]" into an exact positive shape.
DRIVER_CLI_REQUEST="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
DRIVER_INSTALLED_CLI="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy"
SOURCE_MIR_EXECUTION="$ROOT_DIR/src/self_hosted/compiler/driver_source_mir_execution_owner.pgy"
DRIVER_BOOTSTRAP="$ROOT_DIR/tests/self_hosted/parity/driver_bootstrap.sh"
DRIVER_PRESSURE_SHARD="$ROOT_DIR/tests/self_hosted/parity/driver_full_mir_seed_pressure_shard.sh"

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
require 'catch [System.IO.IOException]'
require '$sampleAttempt -le 20'
require 'Start-Sleep -Milliseconds 25'
require 'build-pressure sample append did not complete'
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
require 'observed_stage_count = $observedStageCount'
require 'last_observed_stage = $lastObservedStage'
require 'LastObservedStage()'
require 'ObservedStageCount()'
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
require '$rootCreatedAt = [datetime]$byId[$RootPid].CreationDate'
require '[Math]::Abs(($rootCreatedAt - $StartedAt).TotalSeconds) -gt 5'
require 'reused PID must not adopt an unrelated process tree'
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
grep -Fq 'full driver MIR production requires pressure observation' "$SOURCE_MIR_EXECUTION" \
    || { echo "[build-pressure-contract] source-MIR action lacks its direct-call rejection" >&2; exit 1; }
# The full fixpoint stays reachable only through the exact pressure-owned argv
# shape: the request owner admits it in one 5-argument form and nothing else,
# so any other spelling falls through to the mode's Die.
grep -Fq 'args[2] == "--pressure-owned-full-fixpoint"' "$DRIVER_CLI_REQUEST" \
    || { echo "[build-pressure-contract] full driver binary pressure token drifted" >&2; exit 1; }
grep -Fq 'DriverCliSourceMirPressureArtifact' "$DRIVER_CLI_REQUEST" \
    || { echo "[build-pressure-contract] pressure-owned request lost its own admission" >&2; exit 1; }
# Same move as above: the installed composition root, not the bootstrap main,
# is what asks for pressure observation now.
grep -Fq 'SourceMirPressureObserved' "$DRIVER_INSTALLED_CLI" \
    || { echo "[build-pressure-contract] installed CLI does not request pressure observation" >&2; exit 1; }
grep -Fq 'DriverSourceMirProjectionFromAdmittedRequest(' "$SOURCE_MIR_EXECUTION" \
    || { echo "[build-pressure-contract] source-MIR action lacks stage observation" >&2; exit 1; }
grep -Fq '[driver-pressure-stage]' "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy" \
    || { echo "[build-pressure-contract] full driver pressure stages are not observable" >&2; exit 1; }
MIR_ARTIFACT_OWNER="$ROOT_DIR/src/self_hosted/mir/artifact_lower_owner.pgy"
grep -Fq 'SelfMirProgramFactsFromReadyArtifactObserved(' "$MIR_ARTIFACT_OWNER" \
    || { echo "[build-pressure-contract] MIR fact owner lacks observed production entry" >&2; exit 1; }
grep -Fq 'machine_declaration, observe_pressure' "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy" \
    || { echo "[build-pressure-contract] full driver does not carry pressure observation into MIR facts" >&2; exit 1; }
for stage in iteration-validation generic-specializations domain-projection routine-lowering intent-lowering canonical-ids; do
    grep -Fq "SelfMirArtifactPressureStage(observe_pressure, \"$stage:start\")" "$MIR_ARTIFACT_OWNER" \
        || { echo "[build-pressure-contract] MIR fact stage is not observable: $stage" >&2; exit 1; }
done
grep -Fq '[semantic-body-type-stage]' "$ROOT_DIR/src/self_hosted/semantic/ast_body_type_bundle_owner.pgy" \
    || { echo "[build-pressure-contract] body type pressure stages are not observable" >&2; exit 1; }
grep -Fq '[semantic-initializer-stage]' "$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy" \
    || { echo "[build-pressure-contract] initializer pressure stages are not observable" >&2; exit 1; }
grep -Fq '"--pressure-owned-full-fixpoint"' "$DRIVER_BOOTSTRAP" \
    || { echo "[build-pressure-contract] full driver runner lacks the pressure-owned token" >&2; exit 1; }
grep -Fq '"$BUILD_DIR/driver_oracle.c"' "$DRIVER_BOOTSTRAP" \
    || { echo "[build-pressure-contract] oracle C artifact boundary is absent" >&2; exit 1; }
grep -Fq 'compile_c "driver_oracle"' "$DRIVER_BOOTSTRAP" \
    || { echo "[build-pressure-contract] oracle host compile is not a separate lifetime" >&2; exit 1; }
if grep -Fq -- '--backend=c' "$DRIVER_BOOTSTRAP"; then
    echo "[build-pressure-contract] monolithic native oracle compile returned" >&2
    exit 1
fi
grep -Fq 'focused output must remain under repository root' "$DRIVER_PRESSURE_SHARD" \
    || { echo "[build-pressure-contract] focused pressure output can escape the repository" >&2; exit 1; }
grep -Fq '"$OUTPUT_REL"' "$DRIVER_PRESSURE_SHARD" \
    || { echo "[build-pressure-contract] focused pressure shard can pass an absolute artifact path" >&2; exit 1; }
grep -Fq '*"/Git/"*"/bash.exe")' "$DRIVER_PARITY" \
    || { echo "[build-pressure-contract] full DRV-2 matrix lacks the Git Bash orphan guard" >&2; exit 1; }
grep -Fq 'full matrix requires MSYS2 bash' "$DRIVER_PARITY" \
    || { echo "[build-pressure-contract] full DRV-2 matrix shell diagnostic drifted" >&2; exit 1; }
grep -Fq 'PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER for a focused Git Bash gate' "$DRIVER_PARITY" \
    || { echo "[build-pressure-contract] focused Git Bash escape hatch is undocumented" >&2; exit 1; }

echo "[build-pressure-contract] native/self-host hard ceiling and full-matrix shell guard wired"
