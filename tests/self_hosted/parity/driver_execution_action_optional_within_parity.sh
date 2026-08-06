#!/usr/bin/env bash
# Subject of this gate: the native C and LLVM backends' optional-within
# action contract (stage 1 greps and diagnostics target the native codegen
# owners). Stage 2 exercises the installed self-host driver separately, on
# its own MIR-production surface. Delegated, stage 1 would judge the
# self-host driver against native-owned pins.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

LABEL="self-host-parity:driver-execution-action-optional-within"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1

SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/driver_execution_action_abi_probe.pgy"
C_OWNER="$ROOT_DIR/src/codegen/transpiler_projection_sync.c"
LLVM_OWNER="$ROOT_DIR/src/codegen/llvm_expr_call_methods_world_effect_sync.c"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver_execution_action_optional_within}"
VARIANT="$BUILD_DIR/driver_execution_action_optional_within.pgy"
EXPECTED_PREFIX=$'ok\nartifact-written\n17'
EXPECTED_FILE_CONTENT="driver-action-abi"
case "$BUILD_DIR" in
    "$ROOT_DIR/.tmp/"*) ;;
    *)
        echo "[$LABEL] build directory must remain under $ROOT_DIR/.tmp" >&2
        exit 1
        ;;
esac
mkdir -p "$BUILD_DIR"

for required in "$SOURCE" "$C_OWNER" "$LLVM_OWNER"; do
    [[ -f "$required" ]] || {
        echo "[$LABEL] missing owner or fixture: $required" >&2
        exit 1
    }
done

# The base fixture proves the zone-bound path. This falsifying projection removes
# only the optional action contract field while retaining the world -> zone ->
# subject call shape that used to trigger the false missing-metadata diagnostic.
sed '/^[[:space:]]*within DriverExecutionActionAbiZone[[:space:]]*$/d' \
    "$SOURCE" >"$VARIANT"
if grep -Fq "within DriverExecutionActionAbiZone" "$VARIANT"; then
    echo "[$LABEL] failed to remove the optional within contract" >&2
    exit 1
fi
grep -Fq "authorized by self" "$VARIANT" || {
    echo "[$LABEL] falsifying fixture lost self authority" >&2
    exit 1
}
grep -Fq "return self.execution.probe.Execute(request);" "$VARIANT" || {
    echo "[$LABEL] falsifying fixture lost the world-embedded action call" >&2
    exit 1
}

for owner_and_getter in \
    "$C_OWNER|transpiler_mir_decl_method_within_zone(method_meta)" \
    "$LLVM_OWNER|llvm_mir_decl_method_within_zone(method_meta)"; do
    owner="${owner_and_getter%%|*}"
    getter="${owner_and_getter#*|}"
    grep -Fq "$getter" "$owner" || {
        echo "[$LABEL] backend no longer consumes the MIR within owner: $getter" >&2
        exit 1
    }
done
for forbidden in \
    "MIR-only C path missing within-zone metadata for self-authorized action" \
    "MIR-only LLVM path missing within-zone metadata for self-authorized action"; do
    if grep -Fq "$forbidden" "$C_OWNER" "$LLVM_OWNER"; then
        echo "[$LABEL] optional within was made mandatory again: $forbidden" >&2
        exit 1
    fi
done

run_self_host_stage() {
    local DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
    if [[ -z "$DRIVER" ]]; then
        echo "[$LABEL] stage 2/2 self-host MIR: SKIP (no prebuilt driver)"
        return 0
    fi
    echo "[$LABEL] stage 2/2 self-host MIR: RUN"
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "$LABEL:self-host" "$DRIVER" || exit 1
    SELF_SOURCE="$BUILD_DIR/self_optional_within.pgy"
    SELF_MIR="$BUILD_DIR/self.optional-within.mir.json"
    cat >"$SELF_SOURCE" <<'PGY'
subject OptionalWithinProbe {
    let value: Int;

    action Execute(self) -> Int
        authorized by self
    {
        return self.value;
    }
}

func Main() -> Void {
    let probe = OptionalWithinProbe(1);
    probe.Execute();
}
PGY
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "${SELF_SOURCE#"$ROOT_DIR"/}" >"$SELF_MIR") || {
        echo "[$LABEL] self-host MIR production failed" >&2
        exit 1
    }
    "${PYTHON_BIN:-python3}" - "$SELF_MIR" <<'PY'
import json
from pathlib import Path
import sys

source = Path(sys.argv[1])
for line in source.read_text(encoding="utf-8").splitlines():
    if line.lstrip().startswith('{"schema":"pgy.mir.v1"'):
        document = json.loads(line)
        break
else:
    raise SystemExit("missing pgy.mir.v1 document")

probe = next(row for row in document["decls"]
             if row.get("name") == "OptionalWithinProbe")
execute = next(row for row in probe["methods"]
               if row.get("name") == "Execute")
contract = execute["contract"]
assert contract["within"] is None, contract
assert contract["authorized_by"] == ["self"], contract
PY
}

run_backend() {
    local backend="$1"
    local bin="$BUILD_DIR/optional_within_${backend}.exe"
    local compile_log="$BUILD_DIR/optional_within_${backend}.compile.log"
    local stdout_file="$BUILD_DIR/optional_within_${backend}.out"
    local stderr_file="$BUILD_DIR/optional_within_${backend}.err"
    local artifact="$BUILD_DIR/optional_within_${backend}.artifact.txt"
    local artifact_rel="${artifact#"$ROOT_DIR"/}"
    local observed_prefix

    # The build-dir guard above makes this a bounded test-temp truncation.
    : >"$artifact"

    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$VARIANT")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" \
        >"$compile_log" 2>&1) || {
        echo "[$LABEL] $backend compile failed" >&2
        cat "$compile_log" >&2
        return 1
    }
    (cd "$ROOT_DIR" && "$bin" "$artifact_rel" \
        >"$stdout_file" 2>"$stderr_file") || {
        echo "[$LABEL] $backend runtime failed" >&2
        cat "$stdout_file" "$stderr_file" >&2
        return 1
    }
    [[ ! -s "$stderr_file" ]] || {
        echo "[$LABEL] $backend wrote unexpected stderr" >&2
        cat "$stderr_file" >&2
        return 1
    }
    observed_prefix="$(tr -d '\r' <"$stdout_file" | head -n 3)"
    [[ "$observed_prefix" == "$EXPECTED_PREFIX" ]] || {
        echo "[$LABEL] $backend lost the non-zone action result" >&2
        cat "$stdout_file" >&2
        return 1
    }
    if grep -Fq "DriverExecutionActionAbiZone" "$stdout_file"; then
        echo "[$LABEL] $backend invented zone authority telemetry" >&2
        return 1
    fi
    [[ -f "$artifact" && "$(cat "$artifact")" == "$EXPECTED_FILE_CONTENT" ]] || {
        echo "[$LABEL] $backend lost the action side effect" >&2
        return 1
    }
}

echo "[$LABEL] stage 1/2 static + native C/LLVM: RUN"
run_backend c
run_backend llvm
cmp -s "$BUILD_DIR/optional_within_c.out" \
    "$BUILD_DIR/optional_within_llvm.out" || {
    echo "[$LABEL] C/LLVM optional-within output diverged" >&2
    exit 1
}
run_self_host_stage

echo "[$LABEL] PASS"
