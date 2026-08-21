#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate: native runtime pool backpressure on both backends.
# Delegating would turn a self-host coverage gap into a scheduler regression.
# This is the declared in-process opt-out, never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Windows: pgy.exe needs the mingw/LLVM runtime DLLs on PATH or it dies as a
# silent exit-127 "command not found" under bare Git-bash (WO-RT-3 residue 3).
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SOURCE="$ROOT_DIR/tests/cases/backend_compare/parallel_backpressure_witness/main.pgy"
ITERATIONS="${PGY_BACKPRESSURE_STRESS_ITERATIONS:-64}"
TIMEOUT_SECONDS="${PGY_BACKPRESSURE_STRESS_TIMEOUT_SECONDS:-3}"
PYTHON_BIN="${PYTHON_BIN:-}"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-backpressure-stress.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN=python3
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN=python
    else
        echo "[parallel-backpressure-stress] python is required" >&2
        exit 1
    fi
fi
if ! [[ "$ITERATIONS" =~ ^[1-9][0-9]*$ ]]; then
    echo "[parallel-backpressure-stress] iterations must be a positive integer" >&2
    exit 1
fi

for backend in c llvm; do
    binary="$WORK_DIR/backpressure_$backend"
    (cd "$ROOT_DIR" && "$PGY" "$SOURCE" "--backend=$backend" -o "$binary") \
        >"$WORK_DIR/$backend.compile.stdout" \
        2>"$WORK_DIR/$backend.compile.stderr"

    "$PYTHON_BIN" - "$binary" "$backend" "$ITERATIONS" "$TIMEOUT_SECONDS" <<'PY'
import subprocess
import sys

binary, backend = sys.argv[1], sys.argv[2]
iterations, timeout_seconds = int(sys.argv[3]), float(sys.argv[4])
for iteration in range(1, iterations + 1):
    try:
        result = subprocess.run(
            [binary], capture_output=True, text=True,
            timeout=timeout_seconds, check=False)
    except subprocess.TimeoutExpired as exc:
        raise SystemExit(
            f"backend={backend} iteration={iteration} timed out after "
            f"{timeout_seconds}s") from exc
    if result.returncode != 0 or result.stdout.strip() != "100":
        raise SystemExit(
            f"backend={backend} iteration={iteration} rc={result.returncode} "
            f"stdout={result.stdout!r} stderr={result.stderr!r}")
print(f"[parallel-backpressure-stress] backend={backend} iterations={iterations} ok")
PY
done
