#!/usr/bin/env bash
# Public DIR text is owned by the installed Pergyra semantic/DIR pipeline.
# Native DIR remains only the explicit independent oracle.
# Closed fallbacks: public_dir_native_fallback,
# public_dir_oracle_self_compare, missing_dir_driver_native_retry; canonical composite priority DIR admission and non-Int rejection.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/public_dir_installed"
WORK_DIR="$ROOT_DIR/$WORK_REL"
DIRECT_MODE="--emit-dir-verified"
PYTHON_BIN="${PYTHON:-}"

fail() {
    echo "[self-host-public-dir] $*" >&2
    exit 1
}

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "python3/python is required"
    fi
fi

normalize_dir() {
    "$PYTHON_BIN" - "$1" "$2" <<'PY'
import re
import sys

text = open(sys.argv[1], encoding="utf-8").read()
text = text.replace("\r\n", "\n").replace("\r", "\n")
text = re.sub(
    r" source=\d+ owner_source=\d+",
    " source=<producer-id> owner_source=<producer-id>",
    text,
)
text = re.sub(
    r"(topology\[\d+\].*? source=)\d+",
    r"\1<producer-id>",
    text,
)
open(sys.argv[2], "w", encoding="utf-8", newline="\n").write(text)
PY
}

PGY="$(pgy_select_optional_exe_binary "$PGY")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "$SELF_DRIVER")"
[[ -x "$PGY" ]] || fail "missing public pgy launcher: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "missing installed self-host driver: $SELF_DRIVER"
PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

while IFS='|' read -r case_name source_path; do
    (cd "$ROOT_DIR" && "$SELF_DRIVER" "$DIRECT_MODE" "$source_path") \
        >"$WORK_DIR/$case_name.direct" 2>"$WORK_DIR/$case_name.direct.err"
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE && \
        "$PGY" --dir "$source_path") \
        >"$WORK_DIR/$case_name.public" 2>"$WORK_DIR/$case_name.public.err"
    (cd "$ROOT_DIR" && "$PGY" --native-pipeline --dir "$source_path") \
        >"$WORK_DIR/$case_name.native" 2>"$WORK_DIR/$case_name.native.err"
    cmp -s "$WORK_DIR/$case_name.direct" "$WORK_DIR/$case_name.public" ||
        fail "$case_name public bytes differ from installed DIR"
    normalize_dir "$WORK_DIR/$case_name.direct" "$WORK_DIR/$case_name.direct.norm"
    normalize_dir "$WORK_DIR/$case_name.native" "$WORK_DIR/$case_name.native.norm"
    cmp -s "$WORK_DIR/$case_name.direct.norm" "$WORK_DIR/$case_name.native.norm" ||
        fail "$case_name installed DIR differs from native oracle"
done <<'CASES'
authority|examples/function_clause_order_minimal.pgy
intent-defaults|tests/self_hosted/parity/fixture/dir_intent_defaults.pgy
transfer-move|examples/transfer_move_minimal.pgy
inline-subintent|examples/composite_intent_orchestration_explicit.pgy
priority-composite|examples/composite_intent_orchestration/main.pgy
nested-on-subintent|tests/self_hosted/parity/fixture/intent_nested_call_reachability.pgy
CASES

set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$WORK_REL/missing-driver" \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" --dir \
    examples/function_clause_order_minimal.pgy) \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE && \
    "$PGY" --dir examples/function_clause_order_minimal.pgy --verbose) \
    >"$WORK_DIR/options.out" 2>"$WORK_DIR/options.err"
options_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" "$DIRECT_MODE") \
    >"$WORK_DIR/arity.out" 2>"$WORK_DIR/arity.err"
arity_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" "$DIRECT_MODE" \
    tests/self_hosted/parity/fixture/intent_priority_non_int.pgy) \
    >"$WORK_DIR/priority-invalid.out" 2>"$WORK_DIR/priority-invalid.err"
priority_invalid_rc=$?
set -e

[[ "$missing_rc" -ne 0 && ! -s "$WORK_DIR/missing.out" ]] ||
    fail "missing installed sibling silently entered native DIR"
grep -Fq "self-host driver is unavailable" "$WORK_DIR/missing.err" ||
    fail "missing sibling lost the installed-boundary diagnostic"
! grep -Fq "[pipeline timing]" "$WORK_DIR/missing.err" ||
    fail "missing sibling retried the native pipeline"
[[ "$options_rc" -ne 0 && ! -s "$WORK_DIR/options.out" ]] ||
    fail "unsupported DIR options entered a compiler path"
grep -Fq -- "--dir options are outside" "$WORK_DIR/options.err" ||
    fail "unsupported DIR options lost selector diagnostic"
[[ "$arity_rc" -ne 0 ]] || fail "installed DIR mode accepted no source"
grep -Fq "installed DIR mode requires one source path" \
    "$WORK_DIR/arity.out" "$WORK_DIR/arity.err" ||
    fail "installed DIR arity lost its typed diagnostic"
[[ "$priority_invalid_rc" -ne 0 && ! -s "$WORK_DIR/priority-invalid.err" ]] ||
    fail "non-Int intent priority entered DIR production"
grep -Fq "self-host DIR intent priority expression must be Int" "$WORK_DIR/priority-invalid.out" ||
    fail "non-Int intent priority lost its owned diagnostic"

grep -Fq 'flags.dump_dir' "$ROOT_DIR/src/pgy_driver.c" ||
    fail "public launcher does not select DIR stdout mode"
grep -Fq '"--emit-dir-verified"' \
    "$ROOT_DIR/src/compiler/driver_self_host_selection_owner.c" ||
    fail "public DIR selector lost the installed request spelling"
grep -Fq 'CompileSourceDirTextVerified(source_path)' \
    "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy" ||
    fail "installed DIR request lost its typed renderer"
if grep -Fq 'driver_run_pipeline(' "$ROOT_DIR/src/compiler/self_host_driver.c"; then
    fail "installed sibling launcher regained a native fallback"
fi

echo "[self-host-public-dir] installed Pergyra DIR facts own public --dir and fail closed"
