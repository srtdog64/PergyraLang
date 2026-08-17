#!/usr/bin/env bash
# Public AST dumping is owned by the installed Pergyra parser. The native
# parser remains reachable only through the declared --native-pipeline oracle.
# Registry forbidden-fallback inventory exercised below:
# public_ast_native_fallback, public_ast_oracle_self_compare.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/public_ast_installed"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="src/self_hosted/parser/fixture/arith_let.pgy"
IMPORT_SOURCE="tests/parser_imported_intent_composition/positive_main.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/parser/fixture/arith_let_ast.txt"
LAUNCHER_OWNER="$ROOT_DIR/src/pgy_driver.c"
SELECTION_OWNER="$ROOT_DIR/src/compiler/driver_self_host_selection_owner.c"
SIBLING_OWNER="$ROOT_DIR/src/compiler/self_host_driver.c"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
EXECUTION_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy"

fail() {
    echo "[self-host-public-ast] $*" >&2
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

(cd "$ROOT_DIR" && "$SELF_DRIVER" --ast "$SOURCE") \
    >"$WORK_DIR/direct.out" 2>"$WORK_DIR/direct.err"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    "$PGY" --ast "$SOURCE") \
    >"$WORK_DIR/public.out" 2>"$WORK_DIR/public.err"
cmp -s "$WORK_DIR/direct.out" "$WORK_DIR/public.out" ||
    fail "public --ast differs from the installed Pergyra parser"

(cd "$ROOT_DIR" && "$PGY" --native-pipeline --ast "$SOURCE") \
    >"$WORK_DIR/native.out" 2>"$WORK_DIR/native.err"
normalize "$WORK_DIR/direct.out" "$WORK_DIR/direct.norm"
normalize "$WORK_DIR/native.out" "$WORK_DIR/native.norm"
normalize "$EXPECTED" "$WORK_DIR/expected.norm"
cmp -s "$WORK_DIR/direct.norm" "$WORK_DIR/native.norm" ||
    fail "installed AST differs from the native oracle"
cmp -s "$WORK_DIR/direct.norm" "$WORK_DIR/expected.norm" ||
    fail "installed AST differs from the committed owner fixture"

(cd "$ROOT_DIR" && "$SELF_DRIVER" --ast "$IMPORT_SOURCE") \
    >"$WORK_DIR/import.direct" 2>"$WORK_DIR/import.direct.err"
(cd "$ROOT_DIR" && "$PGY" --ast "$IMPORT_SOURCE") \
    >"$WORK_DIR/import.public" 2>"$WORK_DIR/import.public.err"
(cd "$ROOT_DIR" && "$PGY" --native-pipeline --ast "$IMPORT_SOURCE") \
    >"$WORK_DIR/import.native" 2>"$WORK_DIR/import.native.err"
normalize "$WORK_DIR/import.direct" "$WORK_DIR/import.direct.norm"
normalize "$WORK_DIR/import.native" "$WORK_DIR/import.native.norm"
cmp -s "$WORK_DIR/import.direct" "$WORK_DIR/import.public" ||
    fail "public imported AST differs from the installed Pergyra parser"
cmp -s "$WORK_DIR/import.direct.norm" "$WORK_DIR/import.native.norm" ||
    fail "installed imported AST differs from the native oracle"
grep -Fq 'Intent: ImportedFrontendPipeline' "$WORK_DIR/import.direct" ||
    fail "installed AST did not preserve import composition"

set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$WORK_REL/missing-driver" \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" --ast "$SOURCE") \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    "$PGY" --ast "$SOURCE" --verbose) \
    >"$WORK_DIR/unsupported.out" 2>"$WORK_DIR/unsupported.err"
unsupported_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" --ast) \
    >"$WORK_DIR/arity.out" 2>"$WORK_DIR/arity.err"
arity_rc=$?
set -e

[[ "$missing_rc" -ne 0 && ! -s "$WORK_DIR/missing.out" ]] ||
    fail "missing sibling silently entered native AST production"
grep -Fq "self-host driver is unavailable" "$WORK_DIR/missing.err" ||
    fail "missing sibling did not report the installed boundary"
! grep -Fq "[pipeline timing]" "$WORK_DIR/missing.err" ||
    fail "missing sibling retried through the native pipeline"
[[ "$unsupported_rc" -ne 0 && ! -s "$WORK_DIR/unsupported.out" ]] ||
    fail "unsupported AST options entered a compiler path"
grep -Fq -- "--ast options are outside the installed self-host driver contract" \
    "$WORK_DIR/unsupported.err" || fail "unsupported AST options lost the selector diagnostic"
[[ "$arity_rc" -ne 0 ]] || fail "installed AST request accepted a missing source"
grep -Fq "source AST mode requires exactly one input path" \
    "$WORK_DIR/arity.out" "$WORK_DIR/arity.err" ||
    fail "installed AST arity lost its typed diagnostic"

require_text "$LAUNCHER_OWNER" \
    'if (flags.dump_tokens || flags.dump_ast'
require_text "$LAUNCHER_OWNER" \
    '|| flags.dump_capability_manifest || flags.dump_dir) {'
require_text "$LAUNCHER_OWNER" 'driver_self_host_source_stdout_mode(&flags)'
require_text "$LAUNCHER_OWNER" 'driver_run_self_host_source_stdout('
require_text "$SELECTION_OWNER" '"--emit-capability-manifest-verified"'
require_text "$SIBLING_OWNER" 'strcmp(argv[0], "--ast") == 0'
require_text "$REQUEST_OWNER" 'DriverCliSourceAstStdout(String)'
require_text "$REQUEST_OWNER" 'args[0] == "--ast"'
require_text "$EXECUTION_OWNER" 'Log(ParseRootProgram(source_path));'
grep -Fq 'driver_run_pipeline(' "$SIBLING_OWNER" &&
    fail "installed sibling launcher regained a native pipeline fallback"

echo "[self-host-public-ast] installed Pergyra parser owns public --ast and fails closed"
