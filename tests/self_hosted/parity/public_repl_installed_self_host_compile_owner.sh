#!/usr/bin/env bash
# The native C REPL owns its session UI, but every executable submission must
# use the installed Pergyra C compile/run owner and must never retry native.

set -euo pipefail

ROOT_DIR="${ROOT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)}"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_DIR="$ROOT_DIR/.tmp/self_hosted/public_repl_installed_self_host"
TEMP_DIR="$WORK_DIR/temp"
COUNT_FILE="$WORK_DIR/count.txt"

repl_owner_fail() {
    echo "[self-host-public-repl] $*" >&2
    exit 1
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${DRIVER}.exe"; then
    DRIVER="${DRIVER}.exe"
fi
[[ -x "$PGY" ]] || repl_owner_fail "missing public pgy launcher: $PGY"
[[ -x "$DRIVER" ]] || repl_owner_fail "missing installed self-host driver: $DRIVER"
command -v "$CC" >/dev/null 2>&1 || repl_owner_fail "missing C compiler: $CC"

mkdir -p "$WORK_DIR" "$TEMP_DIR"
rm -f "$WORK_DIR"/*.out "$WORK_DIR"/*.err "$WORK_DIR"/*.txt
rm -f "$TEMP_DIR"/pgy_repl_*

TEMP_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$TEMP_DIR")"
DRIVER_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
COUNT_FILE_FOR_DRIVER="$(pgy_path_for_compiler "$PGY" "$COUNT_FILE")"

set +e
(cd "$ROOT_DIR" &&
    printf 'Log("repl-self-host");\nexit\n' |
    TMPDIR="$TEMP_FOR_PGY" TMP="$TEMP_FOR_PGY" TEMP="$TEMP_FOR_PGY" \
    PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" "$PGY" --repl \
        >"$WORK_DIR/real.out" 2>"$WORK_DIR/real.err")
real_rc=$?
set -e
[[ "$real_rc" -eq 0 ]] || repl_owner_fail "installed REPL evaluation failed"
grep -Fq 'Pergyra REPL v0.1' "$WORK_DIR/real.out" ||
    repl_owner_fail "REPL greeting disappeared"
[[ "$(grep -Foc 'repl-self-host' "$WORK_DIR/real.out")" == "1" ]] ||
    repl_owner_fail "installed REPL program output was not observed exactly once"
grep -Fq 'pgy: compiled' "$WORK_DIR/real.out" ||
    repl_owner_fail "installed REPL compile receipt disappeared"
grep -Fq 'Bye!' "$WORK_DIR/real.out" ||
    repl_owner_fail "REPL completion disappeared"

counting_driver="$WORK_DIR/counting-self-driver"
if [[ "$PGY" == *.exe ]]; then counting_driver="${counting_driver}.exe"; fi
"$CC" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_self_host_c_driver.c" \
    -o "$counting_driver"
COUNTING_DRIVER_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$counting_driver")"
set +e
(cd "$ROOT_DIR" &&
    printf 'Log("counted-repl-evaluation");\nexit\n' |
    TMPDIR="$TEMP_FOR_PGY" TMP="$TEMP_FOR_PGY" TEMP="$TEMP_FOR_PGY" \
    PGY_SELF_DRIVER_BIN="$COUNTING_DRIVER_FOR_PGY" \
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" "$PGY" --repl \
        >"$WORK_DIR/counting.out" 2>"$WORK_DIR/counting.err")
counting_rc=$?
set -e
[[ "$counting_rc" -eq 0 ]] || repl_owner_fail "counting REPL evaluation failed"
[[ "$(wc -l < "$COUNT_FILE" | tr -d ' ')" == "1" ]] ||
    repl_owner_fail "one REPL evaluation did not invoke exactly one installed driver"
grep -Fq 'self-host-shim' "$WORK_DIR/counting.out" ||
    repl_owner_fail "counting installed artifact was not executed"

MISSING_DRIVER_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/does-not-exist")"
set +e
(cd "$ROOT_DIR" &&
    printf 'Log("repl-missing-owner");\nexit\n' |
    TMPDIR="$TEMP_FOR_PGY" TMP="$TEMP_FOR_PGY" TEMP="$TEMP_FOR_PGY" \
    PGY_SELF_DRIVER_BIN="$MISSING_DRIVER_FOR_PGY" "$PGY" --repl \
        >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err")
missing_rc=$?
(cd "$ROOT_DIR" &&
    printf 'Log(missing_repl_name);\nexit\n' |
    TMPDIR="$TEMP_FOR_PGY" TMP="$TEMP_FOR_PGY" TEMP="$TEMP_FOR_PGY" \
    PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" "$PGY" --repl \
        >"$WORK_DIR/invalid.out" 2>"$WORK_DIR/invalid.err")
invalid_rc=$?
set -e
[[ "$missing_rc" -eq 0 && "$invalid_rc" -eq 0 ]] ||
    repl_owner_fail "a rejected evaluation terminated the interactive session"
grep -Fq 'self-host driver is unavailable' "$WORK_DIR/missing.err" ||
    repl_owner_fail "missing installed driver did not fail explicitly"
! grep -Fq 'repl-missing-owner' "$WORK_DIR/missing.out" ||
    repl_owner_fail "missing installed driver retried through native compilation"
! grep -Fq 'pgy: compiled' "$WORK_DIR/missing.out" ||
    repl_owner_fail "missing installed driver published a compile receipt"
grep -Fq 'self-host driver failed' "$WORK_DIR/invalid.err" ||
    repl_owner_fail "invalid REPL input lost the installed-owner failure receipt"
! grep -Fq 'pgy: compiled' "$WORK_DIR/invalid.out" ||
    repl_owner_fail "invalid REPL input published a binary"
for file in "$WORK_DIR/missing.err" "$WORK_DIR/invalid.err"; do
    ! grep -Fq '[pipeline timing]' "$file" ||
        repl_owner_fail "REPL evaluation re-entered the native timed pipeline"
done

! grep -Fq 'driver_run_pipeline(' "$ROOT_DIR/src/compiler/repl.c" ||
    repl_owner_fail "REPL regained a direct native compiler call"
[[ "$(grep -Fc 'c_runner_execute_installed_self_host_c(' \
    "$ROOT_DIR/src/compiler/repl.c")" == "1" ]] ||
    repl_owner_fail "REPL must enter exactly one installed compile/run boundary"
grep -Fq 'return repl_run(argv[0]);' "$ROOT_DIR/src/pgy_driver.c" ||
    repl_owner_fail "launcher identity no longer reaches the REPL compile owner"
if compgen -G "$TEMP_DIR/pgy_repl_*" >/dev/null; then
    repl_owner_fail "REPL left a source or binary artifact"
fi

echo "[self-host-public-repl] installed Pergyra owner compiles each evaluation; C session UI remains native"
