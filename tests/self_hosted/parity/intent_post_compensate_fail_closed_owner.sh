#!/usr/bin/env bash
set -euo pipefail

# Parser support is SURFACE only until guard/post/compensate survive
# DIR -> MIR -> C. The executable driver must reject all three clauses instead
# of silently deleting them and emitting a different program.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-post-compensate] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-post-compensate" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON_BIN:-python3}"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"

BUILD_DIR="${PGY_SELFHOST_INTENT_POST_COMP_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_post_compensate_fail_closed}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
mkdir -p "$BUILD_DIR"

if [[ -n "$DRIVER" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "self-host-intent-post-compensate" "$DRIVER" \
        || fail "prebuilt driver is not runnable"
else
    DRIVER="$BUILD_DIR/driver_rung2.exe"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) \
        || { tail -c 65536 "$BUILD_DIR/driver.compile.log" >&2; fail "driver build failed"; }
fi

SOURCE="$ROOT_DIR/tests/self_hosted/parity/fixture/intent_outcome_frontend.pgy"
POST_ONLY="$BUILD_DIR/intent_post_only.pgy"
GUARD_ONLY="$BUILD_DIR/intent_guard_only.pgy"
"$PYTHON_BIN" - "$SOURCE" "$POST_ONLY" "$GUARD_ONLY" <<'PY'
from pathlib import Path
import sys

text = Path(sys.argv[1]).read_text(encoding="utf-8")
post_only = text.replace("        compensate: ToString(outcome);\n", "")
Path(sys.argv[2]).write_text(post_only, encoding="utf-8")
guard_only = post_only.replace(
    "        post: outcome + 1;\n", "        guard: outcome == 7;\n"
)
Path(sys.argv[3]).write_text(guard_only, encoding="utf-8")
PY

check_rejected() {
    local label="$1"
    local source_rel="$2"
    local diagnostic="$3"
    local mode="$4"
    local out="$BUILD_DIR/$label.out"
    local err="$BUILD_DIR/$label.err"
    local accepted=0
    if [[ "$mode" == "mir" ]]; then
        (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
            "$source_rel" >"$out" 2>"$err") && accepted=1
    else
        (cd "$ROOT_DIR" && "$DRIVER" "$source_rel" --emit-c-verified \
            >"$out" 2>"$err") && accepted=1
    fi
    if [[ "$accepted" -eq 1 ]]; then
        fail "$label was accepted"
    fi
    grep -Fq "$diagnostic" "$out" "$err" \
        || { cat "$out" "$err" >&2; fail "$label diagnostic drifted"; }
    if grep -Eq '^#include|"schema"[[:space:]]*:' "$out" "$err"; then
        fail "$label emitted a partial artifact before rejection"
    fi
}

FIXTURE_REL="tests/self_hosted/parity/fixture/intent_outcome_frontend.pgy"
POST_REL="${POST_ONLY#"$ROOT_DIR/"}"
GUARD_REL="${GUARD_ONLY#"$ROOT_DIR/"}"
check_rejected compensate-mir "$FIXTURE_REL" \
    "self-host DIR intent compensate carrier is not executable" \
    mir
check_rejected compensate-c "$FIXTURE_REL" \
    "self-host DIR intent compensate carrier is not executable" \
    c
check_rejected post-mir "$POST_REL" \
    "self-host DIR intent post carrier is not executable" \
    mir
check_rejected post-c "$POST_REL" \
    "self-host DIR intent post carrier is not executable" \
    c
check_rejected guard-mir "$GUARD_REL" \
    "self-host DIR intent guard carrier is not executable" \
    mir
check_rejected guard-c "$GUARD_REL" \
    "self-host DIR intent guard carrier is not executable" \
    c

echo "[self-host-intent-post-compensate] parser surface cannot silently cross the executable boundary: PASS"
