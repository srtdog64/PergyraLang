#!/usr/bin/env bash
# Builds the bounded DRV-2 compiler with the Pergyra parser/codegen seeds.
# The native compiler may build stage-0 seeds, but it must not compile the
# replacement driver source directly.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
export PATH

case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*)
        # Preserve repo-relative Pergyra paths for SelfHostPath. MSYS argv
        # conversion would rewrite them before the program resolves imports.
        PGY_ARG_CONV_EXCL="*"
        ;;
    *) PGY_ARG_CONV_EXCL="" ;;
esac

CC="${PGY_SELFHOST_CC:-gcc}"
CODEGEN_BUILD="${PGY_SELFHOST_CODEGEN_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/codegen/bootstrap}"
BUILD_DIR="${PGY_SELFHOST_COMPILER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/compiler/bootstrap}"
PARSER_BIN="${PGY_SELFHOST_PARSER_SEED:-$CODEGEN_BUILD/parser_ast_producer.exe}"
CODEGEN_BIN="${PGY_SELFHOST_CODEGEN_SEED:-$CODEGEN_BUILD/gen2.exe}"
DRIVER_SOURCE="src/self_hosted/compiler/driver_bootstrap_main.pgy"
OUTPUT="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
AST_FILE="$BUILD_DIR/driver.ast.txt"
C_FILE="$BUILD_DIR/driver.c"
STAMP="$BUILD_DIR/driver.build.key"
EMIT_STAMP="$BUILD_DIR/driver.emit.key"
KEY_INPUT="$BUILD_DIR/driver.build.key.input"
AST_ERROR="$BUILD_DIR/driver.ast.err"
SMOKE_OUT="$BUILD_DIR/driver.smoke.c"

# The stamp owns one installed driver artifact, not just its input graph. A
# cache directory may be reused with a different output path during isolated
# bootstrap checks, so bind the output identity before accepting a prior stamp.
OUTPUT_KEY="$OUTPUT"
case "$OUTPUT_KEY" in
    "$ROOT_DIR"/*) OUTPUT_KEY="${OUTPUT_KEY#"$ROOT_DIR"/}" ;;
esac

case "$OUTPUT" in
    *.exe) ;;
    *)
        if pgy_binary_expects_windows_paths "$PARSER_BIN"; then
            OUTPUT="${OUTPUT}.exe"
        fi
        ;;
esac

fail() {
    echo "[self-host-compiler-build] $*" >&2
    exit 1
}

hash_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | cut -d' ' -f1
        return
    fi
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | cut -d' ' -f1
        return
    fi
    fail "no SHA-256 tool is available"
}

mkdir -p "$BUILD_DIR" "$(dirname "$OUTPUT")"
[[ -f "$ROOT_DIR/$DRIVER_SOURCE" ]] || fail "missing driver source"
[[ -f "$PARSER_BIN" ]] || fail "missing parser seed: $PARSER_BIN"
[[ -f "$CODEGEN_BIN" ]] || fail "missing Pergyra codegen seed: $CODEGEN_BIN"
pgy_require_runnable_binary_here "self-host-compiler-build" "$PARSER_BIN" || exit 1
pgy_require_runnable_binary_here "self-host-compiler-build" "$CODEGEN_BIN" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"

# The composed AST is the parser owner's import-graph fact. Hashing a stale
# parser-build inventory here allowed the installed driver to retain 76 MIR
# fixtures after the live owner had reached 109. Parse first on every build;
# the expensive codegen/host-compile legs remain fingerprint-cached.
echo "[self-host-compiler-build] parsing DRV-2 composed source graph"
rm -f "$AST_FILE" "$AST_ERROR"
if ! (cd "$ROOT_DIR" && MSYS2_ARG_CONV_EXCL="$PGY_ARG_CONV_EXCL" \
    "$PARSER_BIN" "$DRIVER_SOURCE" 2>"$AST_ERROR" \
    | tr -d '\r' >"$AST_FILE"); then
    tail -n 20 "$AST_FILE" "$AST_ERROR" >&2 || true
    fail "Pergyra parser seed rejected the DRV-2 source graph"
fi
[[ -s "$AST_FILE" ]] || fail "Pergyra parser seed emitted an empty AST"
ast_rel="${AST_FILE#"$ROOT_DIR"/}"

printf '%s\n' \
    "schema=pgy.selfhost.compiler-build.v2" \
    "parser=$(hash_file "$PARSER_BIN")" \
    "codegen=$(hash_file "$CODEGEN_BIN")" \
    "composed_ast=$(hash_file "$AST_FILE")" \
    "output=$OUTPUT_KEY" \
    "cc=$($CC --version 2>/dev/null | head -1)" \
    >"$KEY_INPUT"
build_key="$(hash_file "$KEY_INPUT")"

if [[ -x "$OUTPUT" && -f "$STAMP" ]] \
    && grep -Fxq "$build_key" "$STAMP" \
    && pgy_binary_is_runnable_here "$OUTPUT"; then
    echo "[self-host-compiler-build] reusing fingerprinted Pergyra-built driver"
    exit 0
fi

if [[ -s "$C_FILE" && -f "$EMIT_STAMP" ]] \
    && grep -Fxq "$build_key" "$EMIT_STAMP"; then
    echo "[self-host-compiler-build] reusing fingerprinted Pergyra-emitted driver C"
else
    echo "[self-host-compiler-build] emitting DRV-2 with Pergyra-built gen2 codegen"
    if ! (cd "$ROOT_DIR" && MSYS2_ARG_CONV_EXCL="$PGY_ARG_CONV_EXCL" \
        "$CODEGEN_BIN" "$ast_rel" | tr -d '\r' >"$C_FILE"); then
        fail "Pergyra-built codegen rejected the DRV-2 AST"
    fi
    if grep -q '^CODEGEN ERROR' "$C_FILE"; then
        grep '^CODEGEN ERROR' "$C_FILE" | head -5 >&2
        fail "DRV-2 is outside the Pergyra codegen subset"
    fi
    [[ -s "$C_FILE" ]] || fail "Pergyra-built codegen emitted empty C"
    printf '%s\n' "$build_key" >"$EMIT_STAMP"
fi

tmp_output="${OUTPUT}.tmp"
rm -f "$tmp_output"
pgy_selfhost_select_emitted_c_compile_profile || fail self-host-emitted-c-profile-invalid
compile_command=("$CC" -x c -std=c11)
compile_command+=(${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]})
if pgy_selfhost_emitted_c_uses_runtime_headers "$C_FILE"; then
    compile_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
compile_command+=("$C_FILE" -o "$tmp_output")
if ! "${compile_command[@]}" >"$BUILD_DIR/driver.compile.log" 2>&1; then
    tail -n 40 "$BUILD_DIR/driver.compile.log" >&2 || true
    fail "emitted DRV-2 C failed to compile"
fi
mv -f "$tmp_output" "$OUTPUT"

smoke_rel="${SMOKE_OUT#"$ROOT_DIR"/}"
rm -f "$SMOKE_OUT"
if ! (cd "$ROOT_DIR" && MSYS2_ARG_CONV_EXCL="$PGY_ARG_CONV_EXCL" "$OUTPUT" \
    src/self_hosted/semantic/fixture/valid_call_int.pgy "$smoke_rel"); then
    fail "Pergyra-built DRV-2 failed its bounded source smoke"
fi
[[ -s "$SMOKE_OUT" ]] || fail "Pergyra-built DRV-2 emitted no smoke artifact"
printf '%s\n' "$build_key" >"$STAMP"
echo "[self-host-compiler-build] Pergyra-built DRV-2 installed: $OUTPUT"
