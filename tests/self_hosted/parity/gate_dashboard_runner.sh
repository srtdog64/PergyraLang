#!/usr/bin/env bash
# Thin process runner for the Pergyra-owned gate dashboard manifest.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_process_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-gate-dashboard-runner"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }

TIER="${PGY_GATE_DASHBOARD_TIER:-static}"
case "$TIER" in static|focused|integration|all) ;; *)
    echo "[$LABEL] tier must be static, focused, integration, or all" >&2
    exit 1
esac

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/gate_dashboard_runner}"
SOURCE="$ROOT_DIR/src/self_hosted/tools/gate_dashboard/main.pgy"
BIN="$BUILD_DIR/gate_dashboard.exe"
MANIFEST="$BUILD_DIR/manifest.tsv"
RESULTS="$BUILD_DIR/results.tsv"
mkdir -p "$BUILD_DIR"

(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$SOURCE")" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$BIN")" \
    >"$BUILD_DIR/compile.log" 2>&1) || {
    echo "[$LABEL] dashboard compile failed" >&2
    cat "$BUILD_DIR/compile.log" >&2
    exit 1
}
(cd "$ROOT_DIR" && "$BIN" --manifest | tr -d '\r' >"$MANIFEST")

MAKE_BIN="${MAKE:-}"
if [[ -z "$MAKE_BIN" ]]; then
    if command -v mingw32-make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v mingw32-make)"
    elif command -v make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v make)"
    else
        echo "[$LABEL] missing make" >&2
        exit 1
    fi
fi

printf 'schema=%s\n' 'pgy.selfhost.gate-results.v1' >"$RESULTS"
executed=0
while IFS= read -r line; do
    [[ -n "$line" && "$line" != schema=* ]] || continue
    IFS=$'\t' read -r gate_id make_target gate_tier budget state blocking owner extra <<<"$line"
    [[ -z "${extra:-}" && -n "$owner" ]] ||
        { echo "[$LABEL] malformed manifest row: $line" >&2; exit 1; }
    if [[ "$TIER" != "all" && "$gate_tier" != "$TIER" ]]; then
        continue
    fi
    log="$BUILD_DIR/${gate_id}.log"
    stdout_log="$BUILD_DIR/${gate_id}.out"
    stderr_log="$BUILD_DIR/${gate_id}.err"
    start_seconds=$SECONDS
    set +e
    (
        cd "$ROOT_DIR"
        export MAKEFLAGS=
        export PGY_BIN="$PGY"
        pgy_run_with_timeout \
            "$budget" "$stdout_log" "$stderr_log" \
            "$MAKE_BIN" --no-print-directory "$make_target"
    )
    rc=$?
    set -e
    cat "$stdout_log" "$stderr_log" >"$log"
    duration_ms=$(((SECONDS - start_seconds) * 1000))
    run_state="PASS"
    detail="ok"
    if [[ "$rc" -eq 124 ]]; then
        run_state="FAIL"
        detail="timeout_${budget}s"
        echo "[$LABEL] $gate_id exceeded its ${budget}s budget" >&2
        tail -n 40 "$log" >&2 || true
    elif [[ "$rc" -ne 0 ]]; then
        run_state="FAIL"
        detail="exit_${rc}"
        echo "[$LABEL] $gate_id failed; tail follows" >&2
        tail -n 40 "$log" >&2 || true
    fi
    printf '%s\t%s\t%s\t%s\n' \
        "$gate_id" "$run_state" "$duration_ms" "$detail" >>"$RESULTS"
    executed=$((executed + 1))
done <"$MANIFEST"

[[ "$executed" -gt 0 ]] || { echo "[$LABEL] selected tier is empty" >&2; exit 1; }
(cd "$ROOT_DIR" && "$BIN" --results "${RESULTS#"$ROOT_DIR/"}")
echo "[$LABEL] tier=$TIER executed=$executed"
