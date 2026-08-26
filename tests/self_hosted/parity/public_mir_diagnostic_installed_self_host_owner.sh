#!/usr/bin/env bash
# Public --mir is one admitted Pergyra diagnostic projection. Native lifecycle
# output remains available only through the explicit --native-pipeline opt-out.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_text_mutation_helpers.sh"
source "$ROOT_DIR/tests/portable_process_helpers.sh"
pgy_prepend_windows_runtime_paths

if [[ "${PGY_PUBLIC_MIR_DIAGNOSTIC_WATCHDOG:-0}" != 1 ]]; then
    WATCHDOG_DIR="$ROOT_DIR/.tmp/self_hosted/public_mir_diagnostic_watchdog"
    mkdir -p "$WATCHDOG_DIR"
    export PGY_PUBLIC_MIR_DIAGNOSTIC_WATCHDOG=1
    set +e
    pgy_run_with_timeout 300 "$WATCHDOG_DIR/stdout" "$WATCHDOG_DIR/stderr" \
        "$BASH" "${BASH_SOURCE[0]}"
    watchdog_rc=$?
    set -e
    [[ ! -s "$WATCHDOG_DIR/stdout" ]] || cat "$WATCHDOG_DIR/stdout"
    [[ ! -s "$WATCHDOG_DIR/stderr" ]] || cat "$WATCHDOG_DIR/stderr" >&2
    if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then return "$watchdog_rc"; fi
    exit "$watchdog_rc"
fi

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/public_mir_diagnostic_installed"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SIMPLE="examples/hello.pgy"
CFG="src/self_hosted/mir_lower/fixture/if_else_assign.pgy"
LAUNCHER="$ROOT_DIR/src/pgy_driver.c"
C_OWNER="$ROOT_DIR/src/compiler/self_host_mir_diagnostic_stdout_owner.c"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
READ_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy"
STDOUT_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_mir_stdout_execution_owner.pgy"
PROJECTION_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/mir_diagnostic_projection_owner.pgy"
INPUT_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/mir_json_input_owner.pgy"

fail() {
    echo "[self-host-public-mir-diagnostic] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
    SELF_DRIVER="${SELF_DRIVER}.exe"
fi
[[ -x "$PGY" ]] || fail "public launcher is missing: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "installed self-host driver is missing: $SELF_DRIVER"
[[ "$(cd "$(dirname "$PGY")" && pwd)" == \
    "$(cd "$(dirname "$SELF_DRIVER")" && pwd)" ]] ||
    fail "public launcher and installed self-host driver are not siblings"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

for row in "simple:$SIMPLE" "cfg:$CFG"; do
    label="${row%%:*}"
    source_path="${row#*:}"
    (cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-diagnostic-verified \
        "$source_path") >"$WORK_DIR/$label.direct" \
        2>"$WORK_DIR/$label.direct.err" ||
        fail "direct installed diagnostic failed: $label"
    (cd "$ROOT_DIR" && env -u PGY_SELF_DRIVER_BIN -u PGY_NATIVE_PIPELINE \
        "$PGY" --mir "$source_path") >"$WORK_DIR/$label.public" \
        2>"$WORK_DIR/$label.public.err" ||
        fail "public diagnostic failed: $label"
    cmp -s "$WORK_DIR/$label.direct" "$WORK_DIR/$label.public" ||
        fail "public diagnostic bytes differ from installed owner: $label"
    grep -Fxq 'Pergyra MIR diagnostic' "$WORK_DIR/$label.public" ||
        fail "diagnostic header is missing: $label"
    grep -Fq 'schema: pgy.mir.diagnostic.v1' "$WORK_DIR/$label.public" ||
        fail "diagnostic schema is missing: $label"
    grep -Fq 'mir-schema: pgy.mir.v1' "$WORK_DIR/$label.public" ||
        fail "canonical MIR schema is missing: $label"
    if grep -Eq 'noncfg-fallbacks|cleanup-block|liveIn=|source-ast-id' \
        "$WORK_DIR/$label.public"; then
        fail "diagnostic guessed a native lifecycle/source fact: $label"
    fi
done

grep -Fq 'routines: 1' "$WORK_DIR/simple.public" ||
    fail "simple routine count is missing"
grep -Fq 'routine[0] kind="function" owner=- name="Main"' \
    "$WORK_DIR/simple.public" || fail "simple routine identity is missing"
grep -Fq 'blocks=4 instructions=6' "$WORK_DIR/cfg.public" ||
    fail "CFG inventory is missing"
grep -Fq 'block[0] reachable=yes succ-true=1 succ-false=2 instructions=2' \
    "$WORK_DIR/cfg.public" || fail "CFG successor facts are missing"
grep -Fq 'kind="phi"' "$WORK_DIR/cfg.public" ||
    fail "CFG phi instruction is missing"

(cd "$ROOT_DIR" && "$PGY" --native-pipeline --mir "$SIMPLE") \
    >"$WORK_DIR/native" 2>"$WORK_DIR/native.err" ||
    fail "explicit native MIR oracle failed"
grep -Fq 'MIR Program' "$WORK_DIR/native" ||
    fail "explicit native MIR oracle lost its legacy identity"
grep -Fq 'noncfg-fallbacks:' "$WORK_DIR/native" ||
    fail "explicit native lifecycle output is missing"
cmp -s "$WORK_DIR/native" "$WORK_DIR/simple.public" &&
    fail "public diagnostic silently reused native MIR bytes"

(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-verified "$SIMPLE") \
    >"$WORK_DIR/admitted.mir.json"
pgy_replace_first_literal "$WORK_DIR/admitted.mir.json" \
    "$WORK_DIR/malformed.mir.json" '"schema":"pgy.mir.v1"' \
    '"schema":"pgy.invalid"'
set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --canonicalize-mir-json \
    "$WORK_REL/malformed.mir.json") >"$WORK_DIR/malformed.out" 2>&1
malformed_rc=$?
set -e
[[ "$malformed_rc" -ne 0 ]] || fail "malformed MIR admission succeeded"
grep -Fq 'input is not a pgy.mir.v1 MIR-JSON document' \
    "$WORK_DIR/malformed.out" || fail "malformed MIR lost its schema diagnostic"
! grep -Fq 'Pergyra MIR diagnostic' "$WORK_DIR/malformed.out" ||
    fail "malformed MIR admission emitted a diagnostic projection"

cat >"$WORK_DIR/rejected.pgy" <<'PGY'
func Main() -> Void {
    MissingSourceMirSurface();
}
PGY
set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" --mir "$WORK_REL/rejected.pgy") \
    >"$WORK_DIR/rejected.out" 2>"$WORK_DIR/rejected.err"
rejected_rc=$?
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$WORK_REL/missing-driver" \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" --mir "$SIMPLE") \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" --mir "$SIMPLE" --runtime=none) \
    >"$WORK_DIR/unsupported.out" 2>"$WORK_DIR/unsupported.err"
unsupported_rc=$?
set -e

[[ "$rejected_rc" -ne 0 && ! -s "$WORK_DIR/rejected.out" ]] ||
    fail "invalid source emitted a diagnostic payload"
grep -Fq 'self-host driver failed' "$WORK_DIR/rejected.err" ||
    fail "invalid source lost its child-failure diagnostic"
[[ "$missing_rc" -ne 0 && ! -s "$WORK_DIR/missing.out" ]] ||
    fail "missing installed driver entered native MIR"
grep -Fq 'self-host driver is unavailable' "$WORK_DIR/missing.err" ||
    fail "missing driver lost its explicit diagnostic"
! grep -Fq '[pipeline timing]' "$WORK_DIR/missing.err" ||
    fail "missing driver retried the native pipeline"
[[ "$unsupported_rc" -ne 0 && ! -s "$WORK_DIR/unsupported.out" ]] ||
    fail "unsupported --mir options were accepted"
grep -Fq 'outside the installed self-host driver contract' \
    "$WORK_DIR/unsupported.err" || fail "unsupported options lost their diagnostic"

silent_driver="$WORK_DIR/silent-self-driver"
[[ "$PGY" == *.exe ]] && silent_driver="$silent_driver.exe"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/silent_self_host_driver.c" \
    -o "$silent_driver"
set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$silent_driver" \
    "$PGY" --mir "$SIMPLE") >"$WORK_DIR/silent.out" \
    2>"$WORK_DIR/silent.err"
silent_rc=$?
set -e
[[ "$silent_rc" -ne 0 && ! -s "$WORK_DIR/silent.out" ]] ||
    fail "silent-success child was accepted"
grep -Fq 'success without a MIR diagnostic payload' "$WORK_DIR/silent.err" ||
    fail "silent-success child lost its fail-closed diagnostic"
set +e
pgy_run_with_timeout 5 "$WORK_DIR/descendant.out" \
    "$WORK_DIR/descendant.err" "$BASH" -c \
    'cd "$1" && PGY_CAPTURE_BOUNDARY_DESCENDANT=1 PGY_SELF_DRIVER_BIN="$2" exec "$3" --mir "$4"' \
    _ "$ROOT_DIR" "$silent_driver" "$PGY" "$SIMPLE"
descendant_rc=$?
set -e
[[ "$descendant_rc" -ne 0 && "$descendant_rc" -ne 124 && \
    ! -s "$WORK_DIR/descendant.out" ]] ||
    fail "descendant-held stdout escaped the capture ownership boundary"
grep -Fq 'success without a MIR diagnostic payload' \
    "$WORK_DIR/descendant.err" || fail "descendant cleanup lost its diagnostic"
set +e
(cd "$ROOT_DIR" && PGY_CAPTURE_BOUNDARY_CLOSE_STDOUT=1 \
    PGY_SELF_DRIVER_BIN="$silent_driver" "$PGY" --mir "$SIMPLE") \
    >"$WORK_DIR/closed.out" 2>"$WORK_DIR/closed.err"
closed_rc=$?
set -e
[[ "$closed_rc" -ne 0 && ! -s "$WORK_DIR/closed.out" ]] ||
    fail "closed child stdout bypassed the empty-payload boundary"
grep -Fq 'success without a MIR diagnostic payload' "$WORK_DIR/closed.err" ||
    fail "closed child stdout was mistaken for a child exit code"

require_text "$LAUNCHER" 'if (flags.dump_mir) return driver_run_self_host_mir_diagnostic_request(argv[0], &flags);'
native_line="$(grep -n 'if (flags.native_pipeline' "$LAUNCHER" | cut -d: -f1)"
mir_line="$(grep -n 'if (flags.dump_mir) return' "$LAUNCHER" | cut -d: -f1)"
final_line="$(grep -n 'return driver_run_pipeline(&flags);' "$LAUNCHER" | tail -1 | cut -d: -f1)"
((native_line < mir_line && mir_line < final_line)) ||
    fail "default MIR delegation escaped the explicit-native/final-dispatch boundary"
require_text "$C_OWNER" 'pgy_exec_argv_capture_stdout('
require_text "$C_OWNER" '"--emit-mir-diagnostic-verified"'
require_text "$C_OWNER" 'success without a MIR diagnostic payload'
! grep -Eq 'driver_run_pipeline|mir_dump|system\(' "$C_OWNER" ||
    fail "MIR diagnostic relay regained a native/string-shell fallback"
require_text "$REQUEST_OWNER" 'DriverCliSourceMirDiagnosticStdout(String)'
require_text "$REQUEST_OWNER" 'args[0] == "--emit-mir-diagnostic-verified"'
require_text "$READ_OWNER" 'DriverSourceMirDiagnosticPayloadOrDie('
require_text "$STDOUT_OWNER" 'ProduceSourceMirThroughPgyCompilerWorld('
require_text "$STDOUT_OWNER" 'MirJsonAdmitBorrowedText('
! grep -Eq '^(enum|struct|tobject) ' "$STDOUT_OWNER" ||
    fail "source-MIR stdout owner created a second protocol species"
require_text "$INPUT_OWNER" 'func MirJsonAdmitBorrowedTextObserved('
require_text "$INPUT_OWNER" 'return MirJsonAdmitBorrowedTextObserved('
require_text "$PROJECTION_OWNER" 'ref admitted: MirMachineLayerAdmittedJsonInput'
! grep -Eq 'ReadFile\(|CompileSourceTo|MirMachineLayerAdmitJsonInput|mir_dump|MIRProgram' \
    "$PROJECTION_OWNER" || fail "diagnostic projection reopened an old fact owner"
for native_row in cfg_body_dataflow_smoke.sh:3 ir_pipeline_probe.sh:1 \
    sanitizer_compile_smoke.sh:2; do
    native_test="${native_row%%:*}"
    expected_calls="${native_row#*:}"
    mir_calls="$(awk '!/^[[:space:]]*#/ && \
        /\$PGY"?[^#]*--mir([[:space:]>]|$)/ { count++ } \
        END { print count + 0 }' "$ROOT_DIR/tests/$native_test")"
    native_calls="$(awk '!/^[[:space:]]*#/ && \
        /\$PGY"?[^#]*--native-pipeline[[:space:]]+--mir([[:space:]>]|$)/ \
        { count++ } END { print count + 0 }' \
        "$ROOT_DIR/tests/$native_test")"
    [[ "$mir_calls" -eq "$expected_calls" && \
        "$native_calls" -eq "$expected_calls" ]] ||
        fail "$native_test has a MIR oracle outside explicit native ownership"
done

echo "[self-host-public-mir-diagnostic] installed Pergyra admission owns public --mir; explicit native lifecycle oracle and failure boundaries: PASS"
