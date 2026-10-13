#!/usr/bin/env bash
set -euo pipefail
# Ratchets source_c_installed_direct_compile_commit.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="$(pgy_path_for_bash_tool "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-cc}"
SOURCE="examples/composite_intent_orchestration/main.pgy"
WORK_REL=".tmp/self_hosted/driver_source_c_execution_action"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[driver-source-c-execution-action] $1" >&2; exit 1; }
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then PGY="${PGY}.exe"; fi
if [[ "$SELF_DRIVER" != *.exe ]] && pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then SELF_DRIVER="${SELF_DRIVER}.exe"; fi
[[ -x "$PGY" ]] || fail "missing public launcher: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "missing installed self-host driver: $SELF_DRIVER"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
MACHINE_MANIFEST="$(pgy_self_driver_machine_manifest_path "$SELF_DRIVER")"
[[ -s "$MACHINE_MANIFEST" ]] || fail "missing installed machine manifest"
# Match the production adapter's repository-relative companion spelling.
MACHINE_MANIFEST_ARG="${MACHINE_MANIFEST#"$ROOT_DIR"/}"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-c-artifact-verified \
    "$SOURCE" "$WORK_REL/direct.c" --machine-manifest-json \
    "$MACHINE_MANIFEST_ARG") || fail "installed driver source-C projection failed"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && \
    "$PGY" "$SOURCE" --emit-c -o "$WORK_REL/public.c") \
    >"$WORK_DIR/public.out" 2>"$WORK_DIR/public.err" || {
        cat "$WORK_DIR/public.out" "$WORK_DIR/public.err" >&2
        fail "public source-C projection failed"; }
cmp -s "$WORK_DIR/direct.c" "$WORK_DIR/public.c" ||
    fail "public source-C artifact differs from the installed driver"

pgy_selfhost_select_emitted_c_compile_profile ||
    fail "emitted C compile profile is invalid"
compile_command=("$CC" -x c -std=c11)
compile_command+=("${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}")
if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK_DIR/public.c"; then
    compile_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
compile_command+=("$WORK_DIR/public.c" -o "$WORK_DIR/public-program")
"${compile_command[@]}"
"$WORK_DIR/public-program" | tr -d '\r' >"$WORK_DIR/public-program.out"
grep -Fxq '[Intent] ProcessOrder=true' "$WORK_DIR/public-program.out" ||
    fail "public source-C execution lost the canonical intent result"
grep -Fxq '[Trace] [intent] enter ProcessOrder' "$WORK_DIR/public-program.out" ||
    fail "public source-C execution lost the canonical trace"

set +e
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && \
    "$PGY" "$SOURCE" --emit-c -o "$WORK_REL/missing-parent/out.c") \
    >"$WORK_DIR/rejected.out" 2>"$WORK_DIR/rejected.err"
rejected_rc=$?
set -e
[[ "$rejected_rc" -ne 0 && ! -e "$WORK_DIR/missing-parent/out.c" ]] ||
    fail "artifact transaction failure published source C"
grep -Fq 'artifact transaction rejected:' "$WORK_DIR/rejected.out" "$WORK_DIR/rejected.err" ||
    fail "artifact transaction failure lost its typed diagnostic"

source "$ROOT_DIR/tests/self_hosted/parity/driver_source_c_stdout_execution_action_gate.sh"

echo "[driver-source-c-execution-action] world/action artifact parity, execution, and transaction rejection: PASS"
