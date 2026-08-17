#!/usr/bin/env bash
# Public token dumping is owned by the installed Pergyra lexer. The native
# lexer remains reachable only through the declared --native-pipeline oracle.
# Registry forbidden-fallback inventory exercised below: public_token_native_fallback, public_token_oracle_self_compare.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/public_tokens_installed"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="examples/hello.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/lexer/fixture/hello_tokens.txt"
LAUNCHER_OWNER="$ROOT_DIR/src/pgy_driver.c"
SELECTION_OWNER="$ROOT_DIR/src/compiler/driver_self_host_selection_owner.c"
SIBLING_OWNER="$ROOT_DIR/src/compiler/self_host_driver.c"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
EXECUTION_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy"

fail() {
    echo "[self-host-public-tokens] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

normalize() {
    pgy_selfhost_normalize_text_artifact <"$1" >"$2"
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

PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
installed_name="pgy-self-driver"
[[ "$PGY" == *.exe ]] && installed_name="pgy-self-driver.exe"
[[ "$SELF_DRIVER" == "$(dirname "$PGY")/$installed_name" ]] ||
    fail "self-host driver is not installed beside the public launcher"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

(cd "$ROOT_DIR" && "$SELF_DRIVER" --tokens "$SOURCE") \
    >"$WORK_DIR/direct.out" 2>"$WORK_DIR/direct.err"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    "$PGY" --tokens "$SOURCE") \
    >"$WORK_DIR/public.out" 2>"$WORK_DIR/public.err"
cmp -s "$WORK_DIR/direct.out" "$WORK_DIR/public.out" ||
    fail "public --tokens differs from the installed Pergyra lexer"

(cd "$ROOT_DIR" && "$PGY" --native-pipeline --tokens "$SOURCE") \
    >"$WORK_DIR/native.out" 2>"$WORK_DIR/native.err"
normalize "$WORK_DIR/direct.out" "$WORK_DIR/direct.norm"
normalize "$WORK_DIR/native.out" "$WORK_DIR/native.norm"
normalize "$EXPECTED" "$WORK_DIR/expected.norm"
cmp -s "$WORK_DIR/direct.norm" "$WORK_DIR/native.norm" ||
    fail "installed token stream differs from the native oracle"
cmp -s "$WORK_DIR/direct.norm" "$WORK_DIR/expected.norm" ||
    fail "installed token stream differs from the committed owner fixture"

set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$WORK_REL/missing-driver" \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" --tokens "$SOURCE") \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    "$PGY" --tokens "$SOURCE" --verbose) \
    >"$WORK_DIR/unsupported.out" 2>"$WORK_DIR/unsupported.err"
unsupported_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" --tokens) \
    >"$WORK_DIR/arity.out" 2>"$WORK_DIR/arity.err"
arity_rc=$?
set -e

[[ "$missing_rc" -ne 0 && ! -s "$WORK_DIR/missing.out" ]] ||
    fail "missing sibling silently entered native token production"
grep -Fq "self-host driver is unavailable" "$WORK_DIR/missing.err" ||
    fail "missing sibling did not report the installed boundary"
! grep -Fq "[pipeline timing]" "$WORK_DIR/missing.err" ||
    fail "missing sibling retried through the native pipeline"
[[ "$unsupported_rc" -ne 0 && ! -s "$WORK_DIR/unsupported.out" ]] ||
    fail "unsupported token options entered a compiler path"
grep -Fq -- "--tokens options are outside the installed self-host driver contract" \
    "$WORK_DIR/unsupported.err" || fail "unsupported token options lost the selector diagnostic"
[[ "$arity_rc" -ne 0 ]] ||
    fail "installed token request accepted a missing source"
grep -Fq "source token mode requires exactly one input path" \
    "$WORK_DIR/arity.out" "$WORK_DIR/arity.err" ||
    fail "installed token arity lost its typed diagnostic"

require_text "$LAUNCHER_OWNER" \
    'if (flags.dump_tokens || flags.dump_ast'
require_text "$LAUNCHER_OWNER" \
    '|| flags.dump_capability_manifest || flags.dump_dir) {'
require_text "$LAUNCHER_OWNER" 'driver_self_host_source_stdout_mode(&flags)'
require_text "$LAUNCHER_OWNER" 'driver_run_self_host_source_stdout('
require_text "$SELECTION_OWNER" 'driver_self_host_source_stdout_mode('
require_text "$SIBLING_OWNER" 'strcmp(argv[0], "--tokens") == 0'
require_text "$REQUEST_OWNER" 'DriverCliSourceTokensStdout(String)'
require_text "$REQUEST_OWNER" 'args[0] == "--tokens"'
require_text "$EXECUTION_OWNER" 'Log(LexContent(source_path, LexerReadSource(source_path)));'
grep -Fq 'driver_run_pipeline(' "$SIBLING_OWNER" &&
    fail "installed sibling launcher regained a native pipeline fallback"

echo "[self-host-public-tokens] installed Pergyra lexer owns public --tokens and fails closed"
