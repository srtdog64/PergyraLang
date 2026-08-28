#!/usr/bin/env bash
# The public debugger must delegate the complete interactive session to the
# installed Pergyra driver exactly once. Native parse/semantic/AST walking is
# forbidden after this boundary.

set -euo pipefail

ROOT_DIR="${ROOT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)}"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_DIR="$ROOT_DIR/.tmp/self_hosted/public_debug_installed_self_host"
COUNT_FILE="$WORK_DIR/count.txt"
LAUNCHER="$ROOT_DIR/src/pgy_driver.c"
DEBUG_ADAPTER="$ROOT_DIR/src/compiler/debugger.c"
DEBUG_HANDOFF="$ROOT_DIR/src/compiler/self_host_debug_driver.c"
CLI_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
EXEC_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy"
SESSION_OWNER="$ROOT_DIR/src/self_hosted/debug/session_owner.pgy"
LOCATION_OWNER="$ROOT_DIR/src/self_hosted/debug/source_location_fact_owner.pgy"

fail() {
    echo "[self-host-public-debug] $*" >&2
    exit 1
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${DRIVER}.exe"; then
    DRIVER="${DRIVER}.exe"
fi
[[ -x "$PGY" ]] || fail "missing public pgy launcher: $PGY"
[[ -x "$DRIVER" ]] || fail "missing installed self-host driver: $DRIVER"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*.out "$WORK_DIR"/*.err "$WORK_DIR"/*.txt

SOURCE="$WORK_DIR/debug_case.pgy"
INVALID_SOURCE="$WORK_DIR/debug_invalid.pgy"
cat >"$SOURCE" <<'EOF'
func Main() -> Void
{
    Log(1);
}
EOF
cat >"$INVALID_SOURCE" <<'EOF'
func Main() -> Void
{
    Log(missing_debug_name);
}
EOF

SOURCE_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$SOURCE")"
INVALID_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$INVALID_SOURCE")"
DRIVER_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$DRIVER")"

set +e
(cd "$ROOT_DIR" && printf 'n\nn\nq\n' |
    PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" "$PGY" debug "$SOURCE_FOR_PGY" \
        >"$WORK_DIR/real.out" 2>"$WORK_DIR/real.err")
real_rc=$?
set -e
[[ "$real_rc" -eq 0 ]] || fail "installed Pergyra debug session failed"
grep -Fq 'Pergyra Debugger v0.1' "$WORK_DIR/real.out" ||
    fail "Pergyra debugger banner disappeared"
grep -Fq 'Commands: n(ext), c(ontinue), b <line>, l(ist), q(uit)' \
    "$WORK_DIR/real.out" || fail "Pergyra debugger command receipt disappeared"
grep -Fq '(pgy-debug:' "$WORK_DIR/real.out" ||
    fail "Pergyra debugger emitted no session prompt"
grep -Fq '(pgy-debug:3) ' "$WORK_DIR/real.out" ||
    fail "parser-owned statement source line did not reach the session"
grep -Fq ':3 | Log(1)' "$WORK_DIR/real.out" ||
    fail "typed statement provenance lost its parser-owned source line"
grep -Fq '0 error(s), 0 warning(s)' \
    <(cat "$WORK_DIR/real.out" "$WORK_DIR/real.err") ||
    fail "typed semantic admission receipt disappeared"

counting_driver="$WORK_DIR/counting-self-debug-driver"
if [[ "$PGY" == *.exe ]]; then counting_driver="${counting_driver}.exe"; fi
"$CC" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_self_host_debug_driver.c" \
    -o "$counting_driver"
COUNTING_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$counting_driver")"
COUNT_FILE_FOR_DRIVER="$(pgy_path_for_compiler "$PGY" "$COUNT_FILE")"
set +e
(cd "$ROOT_DIR" && printf 'q\n' |
    PGY_SELF_DRIVER_BIN="$COUNTING_FOR_PGY" \
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
    "$PGY" debug "$SOURCE_FOR_PGY" \
        >"$WORK_DIR/counting.out" 2>"$WORK_DIR/counting.err")
counting_rc=$?
set -e
[[ "$counting_rc" -eq 0 ]] || fail "counting debug owner was not executable"
[[ -f "$COUNT_FILE" ]] ||
    fail "one public debug session did not invoke the installed owner"
[[ "$(wc -l <"$COUNT_FILE" | tr -d ' ')" == "1" ]] ||
    fail "one public debug session did not invoke exactly one installed owner"
grep -Fq 'debug-session-shim' "$WORK_DIR/counting.out" ||
    fail "public debugger did not expose installed-owner output"

MISSING_FOR_PGY="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/missing-self-driver")"
set +e
(cd "$ROOT_DIR" && printf 'q\n' |
    PGY_SELF_DRIVER_BIN="$MISSING_FOR_PGY" PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" debug "$SOURCE_FOR_PGY" \
        >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err")
missing_rc=$?
(cd "$ROOT_DIR" && printf 'q\n' |
    PGY_SELF_DRIVER_BIN="$DRIVER_FOR_PGY" PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" debug "$INVALID_FOR_PGY" \
        >"$WORK_DIR/invalid.out" 2>"$WORK_DIR/invalid.err")
invalid_rc=$?
set -e
[[ "$missing_rc" -ne 0 ]] || fail "missing installed debugger retried natively"
[[ "$invalid_rc" -ne 0 ]] || fail "invalid source entered a debug session"
[[ ! -s "$WORK_DIR/missing.out" ]] ||
    fail "missing installed debugger emitted partial stdout"
[[ ! -s "$WORK_DIR/invalid.out" ]] ||
    fail "invalid source emitted a debugger banner or prompt"
grep -Fq 'self-host driver is unavailable' "$WORK_DIR/missing.err" ||
    fail "missing owner lost its explicit diagnostic"
for file in "$WORK_DIR/missing.err" "$WORK_DIR/invalid.err"; do
    ! grep -Fq '[pipeline timing]' "$file" ||
        fail "debug failure re-entered the native timed pipeline"
done

grep -Fq 'return driver_run_debug_command(argv[0], argc - 1, argv + 1);' \
    "$LAUNCHER" || fail "launcher identity no longer reaches debug adapter"
[[ "$(grep -Fc 'driver_run_self_host_debug_session(' "$DEBUG_ADAPTER")" == "1" ]] ||
    fail "debug adapter must enter exactly one installed session boundary"
for forbidden in parser_parse_program semantic_analyze debug_walk_statements \
    lexer_create '../parser/parser.h' '../semantic/semantic.h'; do
    ! grep -Fq "$forbidden" "$DEBUG_ADAPTER" ||
        fail "debug adapter retained native path residue: $forbidden"
    ! grep -Fq "$forbidden" "$DEBUG_HANDOFF" ||
        fail "debug handoff retained native path residue: $forbidden"
done
grep -Fq 'DriverCliDebugSession(String)' "$CLI_OWNER" ||
    fail "installed CLI request owner lacks debug-session identity"
grep -Fq 'case DriverCliDebugSession(source_path):' "$EXEC_OWNER" ||
    fail "installed executor does not consume debug-session identity"
[[ -f "$SESSION_OWNER" ]] || fail "Pergyra debug session owner is missing"
[[ "$(grep -Fc 'ParseRootProgramBuild(' "$SESSION_OWNER")" == "1" ]] ||
    fail "debug session must build the parser artifact exactly once"
grep -Fq 'ParserProgramArtifactFromBuild(build)' "$SESSION_OWNER" ||
    fail "debug session rebuilt the admitted artifact outside its parser build"
for forbidden in 'ParseRootProgramArtifact(' 'LoadSemanticSource('; do
    ! grep -Fq "$forbidden" "$SESSION_OWNER" ||
        fail "debug session reopened source after parser admission: $forbidden"
    ! grep -Fq "$forbidden" "$LOCATION_OWNER" ||
        fail "debug location join reopened source: $forbidden"
done

echo "[self-host-public-debug] installed Pergyra owner owns the complete public session"
