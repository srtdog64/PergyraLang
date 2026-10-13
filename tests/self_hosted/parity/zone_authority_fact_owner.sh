#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
BUILD_DIR="${PGY_SELFHOST_ZONE_AUTHORITY_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/zone_authority_fact}"
PROBE="src/self_hosted/tools/zone_authority_fact_probe/main.pgy"

fail() {
    echo "[self-host-zone-authority] $*" >&2
    exit 1
}

mkdir -p "$BUILD_DIR"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-zone-authority" "$PGY" \
    || fail "PGY_BIN is not runnable"
PROBE_BIN="$BUILD_DIR/zone_authority_fact_probe.exe"
(cd "$ROOT_DIR" && "$PGY" "$PROBE" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$PROBE_BIN")" \
    >"$BUILD_DIR/compile.log" 2>&1) \
    || { cat "$BUILD_DIR/compile.log" >&2; fail "probe compile failed"; }
[[ -f "$PROBE_BIN" ]] || fail "probe compiler emitted no executable"
probe_out="$BUILD_DIR/probe.out"
probe_err="$BUILD_DIR/probe.err"
(cd "$ROOT_DIR" && "$PROBE_BIN" >"$probe_out" 2>"$probe_err") \
    || { cat "$probe_out" "$probe_err" >&2; fail "probe rejected contract"; }
grep -Fq 'zone-authority-carriage=PASS' "$probe_out" \
    || { cat "$probe_out" "$probe_err" >&2; fail "probe lost PASS marker"; }

echo "[self-host-zone-authority] semantic identity carriage + admitted mutation negatives + DIR no-rescan: PASS"
