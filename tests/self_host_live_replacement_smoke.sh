#!/usr/bin/env bash
# The public pgy launcher must execute the shipped bounded DRV-2 binary rather
# than silently falling back to the C semantic/codegen pipeline.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/live_replacement}"
mkdir -p "$WORK_DIR"

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] && pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
    SELF_DRIVER="${SELF_DRIVER}.exe"
fi
[[ -x "$PGY" ]] || { echo "[self-host-live] missing pgy: $PGY" >&2; exit 1; }
[[ -x "$SELF_DRIVER" ]] || { echo "[self-host-live] missing self driver: $SELF_DRIVER" >&2; exit 1; }
command -v "$CC" >/dev/null 2>&1 || { echo "[self-host-live] missing C compiler: $CC" >&2; exit 1; }

positive="src/self_hosted/semantic/fixture/valid_call_int.pgy"
negative="src/self_hosted/semantic/fixture/bad_return_type.pgy"

(cd "$ROOT_DIR" && "$SELF_DRIVER" "$positive" --emit-c-verified) >"$WORK_DIR/direct.c"
(cd "$ROOT_DIR" && "$PGY" --self-driver "$positive") >"$WORK_DIR/launcher.c"
cmp -s "$WORK_DIR/direct.c" "$WORK_DIR/launcher.c" || {
    echo "[self-host-live] launcher C artifact differs from direct DRV-2" >&2
    exit 1
}
"$CC" -x c -std=c11 "$WORK_DIR/launcher.c" -o "$WORK_DIR/launcher-program" \
    >"$WORK_DIR/cc.log" 2>&1 || {
        cat "$WORK_DIR/cc.log" >&2
        exit 1
    }

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" "$negative" --emit-c-verified) >"$WORK_DIR/direct.diag" 2>"$WORK_DIR/direct.err"
direct_rc=$?
(cd "$ROOT_DIR" && "$PGY" --self-driver "$negative") >"$WORK_DIR/launcher.diag" 2>"$WORK_DIR/launcher.err"
launcher_rc=$?
set -e
[[ "$direct_rc" -ne 0 && "$launcher_rc" -eq "$direct_rc" ]] || {
    echo "[self-host-live] negative exit code drift: direct=$direct_rc launcher=$launcher_rc" >&2
    exit 1
}
tr -d '\r' <"$WORK_DIR/direct.diag" >"$WORK_DIR/direct.norm"
tr -d '\r' <"$WORK_DIR/launcher.diag" >"$WORK_DIR/launcher.norm"
cmp -s "$WORK_DIR/direct.norm" "$WORK_DIR/launcher.norm" || {
    echo "[self-host-live] launcher diagnostic differs from direct DRV-2" >&2
    exit 1
}

set +e
PGY_SELF_DRIVER_BIN="$WORK_DIR/missing-driver" "$PGY" --self-driver "$positive" \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
set -e
[[ "$missing_rc" -ne 0 ]] || {
    echo "[self-host-live] missing self driver silently fell back" >&2
    exit 1
}
grep -Fq "self-host driver is unavailable" "$WORK_DIR/missing.err" || {
    echo "[self-host-live] missing-driver failure is not explicit" >&2
    exit 1
}

echo "[self-host-live] explicit DRV-2 replacement path is artifact-equal and fail-closed"
