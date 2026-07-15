#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-ast-text-assignment-colon-literal"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
CC="${PGY_SELFHOST_CC:-gcc}"
BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/ast_text_assignment_colon_literal"
PARSER_SOURCE="$ROOT_DIR/src/self_hosted/parser/main.pgy"
CODEGEN_SOURCE="$ROOT_DIR/src/self_hosted/codegen/main.pgy"
FIXTURE="$ROOT_DIR/src/self_hosted/codegen/fixture/assignment_colon_literal.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/codegen/expected/assignment_colon_literal_stdout.txt"
PARSER_BIN="$BUILD_DIR/parser.exe"
CODEGEN_BIN="$BUILD_DIR/codegen.exe"
AST_REL=".tmp/self_hosted/ast_text_assignment_colon_literal/fixture_ast.txt"
AST_FILE="$ROOT_DIR/$AST_REL"
GENERATED_C="$BUILD_DIR/fixture.c"
GENERATED_BIN="$BUILD_DIR/fixture.exe"
ACTUAL="$BUILD_DIR/fixture_stdout.txt"

fail() {
    echo "[$LABEL] $*" >&2
    exit 1
}

[[ -x "$PGY" ]] || fail "missing compiler binary: $PGY"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
mkdir -p "$BUILD_DIR"

compile_pgy_tool() {
    local source="$1"
    local output="$2"
    local log="$3"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$source")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$output")" >"$log" 2>&1); then
        cat "$log" >&2
        fail "tool compile failed: ${source#"$ROOT_DIR"/}"
    fi
}

compile_pgy_tool "$PARSER_SOURCE" "$PARSER_BIN" "$BUILD_DIR/parser_compile.log"
compile_pgy_tool "$CODEGEN_SOURCE" "$CODEGEN_BIN" "$BUILD_DIR/codegen_compile.log"

if ! (cd "$ROOT_DIR" && "$PARSER_BIN" \
    "src/self_hosted/codegen/fixture/assignment_colon_literal.pgy" \
    >"$AST_FILE.raw" 2>"$BUILD_DIR/parser_run.err"); then
    cat "$BUILD_DIR/parser_run.err" >&2
    fail "self-host parser failed"
fi
tr -d '\r' <"$AST_FILE.raw" >"$AST_FILE"
grep -Fq 'Assign: output = Concat("head:", Summary())' "$AST_FILE" ||
    fail "self-host parser AST did not preserve the colon assignment"

if ! (cd "$ROOT_DIR" && "$CODEGEN_BIN" "$AST_REL" \
    >"$GENERATED_C.raw" 2>"$BUILD_DIR/codegen_run.err"); then
    cat "$BUILD_DIR/codegen_run.err" "$GENERATED_C.raw" >&2
    fail "self-host codegen failed"
fi
tr -d '\r' <"$GENERATED_C.raw" >"$GENERATED_C"

grep -Fq 'output = (pgy_concat("head:", Summary()));' "$GENERATED_C" ||
    fail "emitted C did not preserve the complete Concat assignment"
if grep -Fq 'output = ("head");' "$GENERATED_C" ||
    grep -Fq 'output = ("head:");' "$GENERATED_C"; then
    fail "emitted C contains a truncated assignment RHS"
fi

if ! "$CC" "$GENERATED_C" -o "$GENERATED_BIN" \
    2>"$BUILD_DIR/generated_c_compile.log"; then
    cat "$BUILD_DIR/generated_c_compile.log" >&2
    fail "generated C failed to compile"
fi
if ! (cd "$ROOT_DIR" && "$GENERATED_BIN" >"$ACTUAL.raw" \
    2>"$BUILD_DIR/generated_run.err"); then
    cat "$BUILD_DIR/generated_run.err" >&2
    fail "generated fixture failed to run"
fi
tr -d '\r' <"$ACTUAL.raw" >"$ACTUAL"
if ! diff -u "$EXPECTED" "$ACTUAL"; then
    fail "golden stdout mismatch"
fi

echo "[$LABEL] AST, emitted C, and golden stdout ok"
