#!/usr/bin/env bash
set -euo pipefail

# Production action outcome gate. The direct-MIR action must return detached
# tobject payloads through world composition; Main consumes success fields and
# an exact transaction failure payload without recovering authority/freshness.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-fallible-tobject-outcome] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-fallible-tobject-outcome" "$PGY" \
    || fail "PGY_BIN is not runnable"

BUILD_DIR="${PGY_SELFHOST_FALLIBLE_TOBJECT_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/fallible_tobject_outcome}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
SOURCE_REL="examples/hello.pgy"
OUTCOME_FIXTURE_REL="tests/self_hosted/fixtures/action_tobject_outcome_probe.pgy"
mkdir -p "$BUILD_DIR"

if [[ -n "$DRIVER" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "self-host-fallible-tobject-outcome" "$DRIVER" \
        || fail "prebuilt driver is not runnable"
else
    DRIVER="$BUILD_DIR/driver_rung2.exe"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) \
        || { tail -c 65536 "$BUILD_DIR/driver.compile.log" >&2; fail "driver build failed"; }
fi

printf '%s\n' 'ok=7' 'error=9' >"$BUILD_DIR/outcome.expected"
for backend in c llvm; do
    native_exe="$BUILD_DIR/outcome.native.$backend.exe"
    (cd "$ROOT_DIR" && "$PGY" "$OUTCOME_FIXTURE_REL" --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$native_exe")" \
        >"$BUILD_DIR/outcome.native.$backend.compile.log" 2>&1) \
        || { cat "$BUILD_DIR/outcome.native.$backend.compile.log" >&2; fail "native $backend outcome compile failed"; }
    "$native_exe" | tr -d '\r' >"$BUILD_DIR/outcome.native.$backend.run"
    cmp -s "$BUILD_DIR/outcome.expected" "$BUILD_DIR/outcome.native.$backend.run" \
        || fail "native $backend did not consume both tobject payloads"
done

SELF_C_REL="${BUILD_DIR#"$ROOT_DIR"/}/outcome.self.c"
SELF_C="$ROOT_DIR/$SELF_C_REL"
SELF_EXE="$BUILD_DIR/outcome.self.exe"
rm -f "$SELF_C" "$SELF_EXE"
(cd "$ROOT_DIR" && "$DRIVER" --emit-c-artifact-verified \
    "$OUTCOME_FIXTURE_REL" "$SELF_C_REL" \
    >"$BUILD_DIR/outcome.self.emit.out" 2>"$BUILD_DIR/outcome.self.emit.err") \
    || { cat "$BUILD_DIR/outcome.self.emit.out" "$BUILD_DIR/outcome.self.emit.err" >&2; fail "self C outcome emission failed"; }
gcc -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$SELF_C" -o "$SELF_EXE" \
    >"$BUILD_DIR/outcome.self.compile.log" 2>&1 \
    || { cat "$BUILD_DIR/outcome.self.compile.log" >&2; fail "self C outcome compile failed"; }
"$SELF_EXE" | tr -d '\r' >"$BUILD_DIR/outcome.self.run"
cmp -s "$BUILD_DIR/outcome.expected" "$BUILD_DIR/outcome.self.run" \
    || fail "self C did not consume both tobject payloads"

MUTUAL_FIXTURE_REL="tests/self_hosted/fixtures/mutual_subject_action_params.pgy"
MUTUAL_EXE="$BUILD_DIR/mutual-subject-params.c.exe"
(cd "$ROOT_DIR" && "$PGY" "$MUTUAL_FIXTURE_REL" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$MUTUAL_EXE")" \
    >"$BUILD_DIR/mutual-subject-params.compile.log" 2>&1) \
    || { cat "$BUILD_DIR/mutual-subject-params.compile.log" >&2; fail "pointer-carried subject params formed a false declaration cycle"; }
printf '%s\n' '3' 'mutual-subject-action-params' >"$BUILD_DIR/mutual-subject-params.expected"
"$MUTUAL_EXE" | tr -d '\r' >"$BUILD_DIR/mutual-subject-params.run"
cmp -s "$BUILD_DIR/mutual-subject-params.expected" \
    "$BUILD_DIR/mutual-subject-params.run" \
    || fail "mutual subject or host-self method dependency probe drifted"

MIR_REL="${BUILD_DIR#"$ROOT_DIR"/}/hello.mir.json"
SUCCESS_REL="${BUILD_DIR#"$ROOT_DIR"/}/hello.c"
MIR="$ROOT_DIR/$MIR_REL"
SUCCESS="$ROOT_DIR/$SUCCESS_REL"
rm -f "$MIR" "$SUCCESS"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL" >"$BUILD_DIR/mir.out" 2>"$BUILD_DIR/mir.err") \
    || { cat "$BUILD_DIR/mir.out" "$BUILD_DIR/mir.err" >&2; fail "MIR production failed"; }
[[ -s "$MIR" ]] || fail "MIR production emitted no artifact"

(cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=c \
    "$MIR_REL" -o "$SUCCESS_REL" >"$BUILD_DIR/success.out" 2>"$BUILD_DIR/success.err") \
    || { cat "$BUILD_DIR/success.out" "$BUILD_DIR/success.err" >&2; fail "success receipt was rejected"; }
[[ -s "$SUCCESS" ]] || fail "success receipt did not publish the artifact"

MISSING_DIR="$BUILD_DIR/not-created"
[[ ! -e "$MISSING_DIR" ]] || fail "negative path owner already exists: $MISSING_DIR"
FAIL_REL="${MISSING_DIR#"$ROOT_DIR"/}/artifact.c"
if (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=c \
    "$MIR_REL" -o "$FAIL_REL" >"$BUILD_DIR/failure.out" 2>"$BUILD_DIR/failure.err"); then
    fail "artifact begin failure was accepted"
fi
[[ ! -e "$ROOT_DIR/$FAIL_REL" ]] || fail "failure published a partial artifact"

EXPECTED="artifact transaction rejected: schema=pgy.compiler-artifact-transaction.v1 final_path=$FAIL_REL stage=begin-temp status=1 prior_final_preserved=true temp_removed=true"
grep -Fxq -- "$EXPECTED" "$BUILD_DIR/failure.out" \
    || grep -Fxq -- "$EXPECTED" "$BUILD_DIR/failure.err" \
    || { cat "$BUILD_DIR/failure.out" "$BUILD_DIR/failure.err" >&2; fail "caller did not consume the exact failure payload"; }

echo "[self-host-fallible-tobject-outcome] success receipt + exact begin failure payload: PASS"
