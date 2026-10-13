#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[hir-routine-identity] missing compiler binary: $PGY" >&2
    exit 1
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_hir_routine_identity.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

source="$ROOT_DIR/tests/cases/hir_routine_identity/main.pgy"
source_arg="$(pgy_path_for_compiler "$PGY" "$source")"
if ! "$PGY" --native-pipeline --hir "$source_arg" >"$WORK_DIR/hir.out" 2>"$WORK_DIR/hir.err"; then
    cat "$WORK_DIR/hir.err" >&2
    exit 1
fi

if ! grep -Eq 'Helper.*hosted=true.*reachable=false' "$WORK_DIR/hir.out"; then
    echo "[hir-routine-identity] hosted Helper was selected by the callgraph" >&2
    cat "$WORK_DIR/hir.out" >&2
    exit 1
fi
if ! grep -Eq 'Helper.*hosted=false.*reachable=true' "$WORK_DIR/hir.out"; then
    echo "[hir-routine-identity] top-level Helper was not reached" >&2
    cat "$WORK_DIR/hir.out" >&2
    exit 1
fi

resource_source="$ROOT_DIR/tests/cases/slot_contract/positive/plain_read_write_release/main.pgy"
resource_arg="$(pgy_path_for_compiler "$PGY" "$resource_source")"
if ! "$PGY" --native-pipeline --rir "$resource_arg" >"$WORK_DIR/rir.out" 2>"$WORK_DIR/rir.err"; then
    cat "$WORK_DIR/rir.err" >&2
    exit 1
fi
if ! grep -Eq 'identity=verified.*stable=0' "$WORK_DIR/rir.out"; then
    echo "[hir-routine-identity] RIR did not carry a verified HIR stable resource identity" >&2
    cat "$WORK_DIR/rir.out" >&2
    exit 1
fi

echo "[hir-routine-identity] semantic declaration target joins HIR RoutineId without a name fallback"
