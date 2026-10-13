#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate:
#   the semantic layer stopped flagging an unstable raw escape.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_BIN_WAS_DEFAULT=0
if [[ -z "${PGY_BIN:-}" ]]; then
    PGY_BIN="$ROOT_DIR/bin/pgy"
    PGY_BIN_WAS_DEFAULT=1
fi
PGY_BIN="$(pgy_select_optional_exe_binary "$PGY_BIN")"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-raw-escape.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

if grep -R -n "PGY_PTR_" \
    "$ROOT_DIR/src/parser" \
    "$ROOT_DIR/src/semantic" \
    "$ROOT_DIR/src/compiler" \
    "$ROOT_DIR/src/codegen"; then
    echo "[raw-escape-contract] PGY_PTR_* must stay runtime-internal, not a parser/semantic/compiler/codegen public surface" >&2
    exit 1
fi

if [[ ! -x "$PGY_BIN" ]]; then
    if [[ "$PGY_BIN_WAS_DEFAULT" -eq 1 ]]; then
        echo "[raw-escape-contract] SKIP executable probe; compiler binary unavailable"
        exit 0
    fi
    echo "[raw-escape-contract] missing compiler binary: $PGY_BIN" >&2
    exit 1
fi
if ! pgy_binary_is_runnable_here "$PGY_BIN"; then
    if [[ "$PGY_BIN_WAS_DEFAULT" -eq 1 ]]; then
        echo "[raw-escape-contract] SKIP executable probe; binary is not runnable on this host: $PGY_BIN"
        exit 0
    fi
    pgy_require_runnable_binary_here "raw-escape-contract" "$PGY_BIN"
fi
if ! "$PGY_BIN" --help >"$WORK_DIR/pgy-help.out" 2>"$WORK_DIR/pgy-help.err"; then
    if [[ "$PGY_BIN_WAS_DEFAULT" -eq 1 ]]; then
        echo "[raw-escape-contract] SKIP executable probe; compiler binary unavailable"
        exit 0
    fi
    echo "[raw-escape-contract] compiler binary is not runnable: $PGY_BIN" >&2
    cat "$WORK_DIR/pgy-help.err" >&2
    exit 1
fi

cat >"$WORK_DIR/raw_escape.pgy" <<'PGY'
func Main() -> Void {
    let slot: Slot<Int> = ClaimSlot<Int>();
    unsafe {
        let raw = SlotRawPointer(slot);
        Log(1);
    }
    Release(slot);
}
PGY

source_arg="$(pgy_path_for_compiler "$PGY_BIN" "$WORK_DIR/raw_escape.pgy")"

if "$PGY_BIN" "$source_arg" --error-format=json >"$WORK_DIR/out.txt" 2>"$WORK_DIR/err.json"; then
    echo "[raw-escape-contract] expected SlotRawPointer rejection" >&2
    exit 1
fi

for term in \
    "PGY_SEM_RAW_ESCAPE_UNSTABLE" \
    "\"stage\":\"semantic\"" \
    "semantic:raw_escape:unstable" \
    "use-pin-or-wait-for-raw-escape-contract" \
    "unsafe { } is only a lexical escape marker" \
    "scoped unsafe(raw) capability" \
    "typed Pin/Lease views"; do
    if ! grep -Fq "$term" "$WORK_DIR/err.json"; then
        echo "[raw-escape-contract] missing diagnostic term: $term" >&2
        cat "$WORK_DIR/err.json" >&2
        exit 1
    fi
done

echo "[raw-escape-contract] system-tier raw escape is explicitly rejected"
