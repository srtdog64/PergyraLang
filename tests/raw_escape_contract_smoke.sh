#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-raw-escape.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ ! -x "$PGY_BIN" ]]; then
    echo "[raw-escape-contract] missing compiler binary: $PGY_BIN" >&2
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
