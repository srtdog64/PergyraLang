#!/usr/bin/env bash
# The public --emit-c path is owned by the installed Pergyra fixed-point
# driver. A missing or unsupported self-host boundary must fail closed instead
# of returning to the native C semantic/codegen pipeline.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_REL=".tmp/self_hosted/default_c_emit_installed"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="examples/hello.pgy"

fail() {
    echo "[self-host-default-c-emit] $*" >&2
    exit 1
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
    SELF_DRIVER="${SELF_DRIVER}.exe"
fi
[[ -x "$PGY" ]] || fail "missing public pgy launcher: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "missing installed self-host driver: $SELF_DRIVER"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"

PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
installed_name="pgy-self-driver"
if [[ "$PGY" == *.exe ]]; then
    installed_name="pgy-self-driver.exe"
fi
EXPECTED_SELF_DRIVER="$(dirname "$PGY")/$installed_name"
[[ "$SELF_DRIVER" == "$EXPECTED_SELF_DRIVER" ]] ||
    fail "self-host driver is not installed beside the public launcher"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR/direct.c" "$WORK_DIR/launcher.c" \
    "$WORK_DIR/missing.c" "$WORK_DIR/unsupported.c"

(cd "$ROOT_DIR" && "$SELF_DRIVER" "$SOURCE" "$WORK_REL/direct.c")
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && \
    "$PGY" "$SOURCE" --emit-c -o "$WORK_REL/launcher.c") \
    >"$WORK_DIR/launcher.out" 2>"$WORK_DIR/launcher.err"
cmp -s "$WORK_DIR/direct.c" "$WORK_DIR/launcher.c" ||
    fail "public --emit-c artifact differs from the installed self-host driver"
grep -Fq "pgy: wrote $WORK_REL/launcher.c" "$WORK_DIR/launcher.out" ||
    fail "public --emit-c did not report its installed artifact"

"$CC" -x c -std=c11 "$WORK_DIR/launcher.c" -o "$WORK_DIR/hello-program"
"$WORK_DIR/hello-program" | tr -d '\r' >"$WORK_DIR/hello.out"
printf 'Hello, Pergyra!\n' >"$WORK_DIR/hello.expected"
cmp -s "$WORK_DIR/hello.expected" "$WORK_DIR/hello.out" ||
    fail "installed self-host C artifact produced the wrong hello output"

set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$WORK_DIR/missing-driver" \
    "$PGY" "$SOURCE" --emit-c -o "$WORK_REL/missing.c") \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && \
    "$PGY" "$SOURCE" --emit-c --runtime=none -o "$WORK_REL/unsupported.c") \
    >"$WORK_DIR/unsupported.out" 2>"$WORK_DIR/unsupported.err"
unsupported_rc=$?
set -e

[[ "$missing_rc" -ne 0 && ! -e "$WORK_DIR/missing.c" ]] ||
    fail "missing installed driver silently used the native C path"
grep -Fq "self-host driver is unavailable" "$WORK_DIR/missing.err" ||
    fail "missing installed driver did not fail explicitly"
[[ "$unsupported_rc" -ne 0 && ! -e "$WORK_DIR/unsupported.c" ]] ||
    fail "unsupported --emit-c options silently used the native C path"
grep -Fq "outside the installed self-host driver contract" \
    "$WORK_DIR/unsupported.err" ||
    fail "unsupported --emit-c options did not fail at the selection boundary"

echo "[self-host-default-c-emit] installed fixed-point driver owns public --emit-c; native fallback is closed"
