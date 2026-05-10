#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY_BIN_WAS_DEFAULT=0
if [[ -z "${PGY_BIN:-}" ]]; then
    PGY_BIN="$ROOT_DIR/bin/pgy"
    PGY_BIN_WAS_DEFAULT=1
fi
if [[ "$PGY_BIN" != *.exe && -x "${PGY_BIN}.exe" ]]; then
    PGY_BIN="${PGY_BIN}.exe"
fi
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-raw-escape.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

require_term() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        { echo "[raw-escape-contract] $rel missing term: $term" >&2; exit 1; }
}

require_term "src/semantic/type_checker_builtins_nominal.c" "PGY_CODE_SEM_RAW_ESCAPE_UNSTABLE"
require_term "src/semantic/type_checker_builtins_nominal.c" "PGY_CAUSE_RAW_ESCAPE_UNSTABLE"
require_term "src/semantic/type_checker_builtins_nominal.c" "PGY_FIX_USE_PIN_OR_WAIT_FOR_RAW_ESCAPE_CONTRACT"
require_term "src/semantic/type_checker_builtins_nominal.c" "unsafe { } is only a lexical escape marker"
require_term "src/semantic/type_checker_builtins_resolve.c" '{"SlotRawPointer", BUILTIN_SLOT_RAW_POINTER}'
require_term "src/semantic/diag_codes.h" "PGY_SEM_RAW_ESCAPE_UNSTABLE"
require_term "src/semantic/diag_codes.h" "semantic:raw_escape:unstable"

if [[ ! -x "$PGY_BIN" ]]; then
    if [[ "$PGY_BIN_WAS_DEFAULT" -eq 1 ]]; then
        echo "[raw-escape-contract] SKIP executable probe; source contract is gated"
        exit 0
    fi
    echo "[raw-escape-contract] missing compiler binary: $PGY_BIN" >&2
    exit 1
fi
if ! "$PGY_BIN" --help >"$WORK_DIR/pgy-help.out" 2>"$WORK_DIR/pgy-help.err"; then
    if [[ "$PGY_BIN_WAS_DEFAULT" -eq 1 ]]; then
        echo "[raw-escape-contract] SKIP executable probe; source contract is gated"
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

if "$PGY_BIN" "$WORK_DIR/raw_escape.pgy" --error-format=json >"$WORK_DIR/out.txt" 2>"$WORK_DIR/err.json"; then
    echo "[raw-escape-contract] expected SlotRawPointer rejection" >&2
    exit 1
fi

for term in \
    "PGY_SEM_RAW_ESCAPE_UNSTABLE" \
    "\"stage\":\"semantic\"" \
    "semantic:raw_escape:unstable" \
    "use-pin-or-wait-for-raw-escape-contract" \
    "unsafe { } is only a lexical escape marker" \
    "typed Pin/Lease views"; do
    if ! grep -Fq "$term" "$WORK_DIR/err.json"; then
        echo "[raw-escape-contract] missing diagnostic term: $term" >&2
        cat "$WORK_DIR/err.json" >&2
        exit 1
    fi
done

echo "[raw-escape-contract] system-tier raw escape is explicitly rejected"
