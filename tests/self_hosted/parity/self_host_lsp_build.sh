#!/usr/bin/env bash
# Builds the installed LSP diagnostics sibling through the Pergyra-built
# typed-source codegen seed. The native C LSP remains an explicit oracle.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
export PATH

case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*) PGY_ARG_CONV_EXCL="*" ;;
    *) PGY_ARG_CONV_EXCL="" ;;
esac

CC="${PGY_SELFHOST_CC:-gcc}"
CODEGEN_BUILD="${PGY_SELFHOST_CODEGEN_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/codegen/bootstrap}"
BUILD_DIR="${PGY_SELFHOST_LSP_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/lsp/installed}"
CODEGEN_BIN="${PGY_SELFHOST_CODEGEN_SEED:-$CODEGEN_BUILD/gen2.exe}"
LSP_SOURCE="src/self_hosted/lsp/main.pgy"
OUTPUT="${PGY_SELF_LSP_BIN:-$ROOT_DIR/bin/pgy-self-lsp}"
C_FILE="$BUILD_DIR/lsp.c"
C_RAW="$BUILD_DIR/lsp.c.raw"
C_NEXT="$BUILD_DIR/lsp.c.next"
STAMP="$BUILD_DIR/lsp.build.key"
KEY_INPUT="$BUILD_DIR/lsp.build.key.input"
RUNTIME_HEADER_KEY_INPUT="$BUILD_DIR/lsp.runtime-headers.build.key.input"
SMOKE_RAW="$BUILD_DIR/lsp.smoke.raw.json"
SMOKE_OUT="$BUILD_DIR/lsp.smoke.json"
SMOKE_EXPECTED="$ROOT_DIR/src/self_hosted/lsp/expected/valid_int_return.json"

fail() {
    echo "[self-host-lsp-build] $*" >&2
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

verify_installed_lsp_artifact() {
    local binary="$1"

    rm -f "$SMOKE_RAW" "$SMOKE_OUT"
    if ! (cd "$ROOT_DIR" && "$binary" \
        src/self_hosted/lsp/fixture/valid_int_return.pgy >"$SMOKE_RAW"); then
        echo "[self-host-lsp-build] Pergyra-built LSP failed its clean diagnostics smoke" >&2
        return 1
    fi
    tr -d '\r' <"$SMOKE_RAW" >"$SMOKE_OUT"
    if [[ "$(hash_file "$SMOKE_EXPECTED")" != "$(hash_file "$SMOKE_OUT")" ]]; then
        echo "[self-host-lsp-build] Pergyra-built LSP changed its clean diagnostics artifact" >&2
        return 1
    fi
}

case "$OUTPUT" in
    *.exe) ;;
    *)
        if pgy_binary_expects_windows_paths "$CODEGEN_BIN"; then
            OUTPUT="${OUTPUT}.exe"
        fi
        ;;
esac
OUTPUT_KEY="$OUTPUT"
case "$OUTPUT_KEY" in
    "$ROOT_DIR"/*) OUTPUT_KEY="${OUTPUT_KEY#"$ROOT_DIR"/}" ;;
esac

mkdir -p "$BUILD_DIR" "$(dirname "$OUTPUT")"
[[ -f "$ROOT_DIR/$LSP_SOURCE" ]] || fail "missing LSP source"
[[ -f "$SMOKE_EXPECTED" ]] || fail "missing LSP smoke artifact"
[[ -f "$CODEGEN_BIN" ]] || fail "missing Pergyra codegen seed: $CODEGEN_BIN"
pgy_require_runnable_binary_here "self-host-lsp-build" "$CODEGEN_BIN" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
pgy_selfhost_select_emitted_c_compile_profile ||
    fail self-host-emitted-c-profile-invalid

echo "[self-host-lsp-build] emitting LSP diagnostics owner from typed source artifact"
rm -f "$C_RAW" "$C_NEXT"
if ! (cd "$ROOT_DIR" && MSYS2_ARG_CONV_EXCL="$PGY_ARG_CONV_EXCL" \
    "$CODEGEN_BIN" --source "$LSP_SOURCE" >"$C_RAW"); then
    fail "Pergyra-built codegen rejected the LSP source graph"
fi
if ! tr -d '\r' <"$C_RAW" >"$C_NEXT"; then
    fail "Pergyra-built LSP C normalization failed"
fi
rm -f "$C_RAW"
if grep -q '^CODEGEN ERROR' "$C_NEXT"; then
    grep '^CODEGEN ERROR' "$C_NEXT" | head -5 >&2
    fail "LSP diagnostics owner is outside the Pergyra codegen subset"
fi
[[ -s "$C_NEXT" ]] || fail "Pergyra-built codegen emitted empty LSP C"

runtime_header_fingerprint=none
if pgy_selfhost_emitted_c_uses_runtime_headers "$C_NEXT"; then
    : >"$RUNTIME_HEADER_KEY_INPUT"
    while IFS= read -r runtime_header; do
        runtime_header_key="${runtime_header#"$ROOT_DIR"/}"
        printf '%s=%s\n' "$runtime_header_key" "$(hash_file "$runtime_header")" \
            >>"$RUNTIME_HEADER_KEY_INPUT"
    done < <(find "$ROOT_DIR/src/runtime" -type f -name '*.h' -print | LC_ALL=C sort)
    runtime_header_fingerprint="$(hash_file "$RUNTIME_HEADER_KEY_INPUT")"
else
    rm -f "$RUNTIME_HEADER_KEY_INPUT"
fi

printf '%s\n' \
    "schema=pgy.selfhost.lsp-build.v1-source-artifact" \
    "source_artifact_c=$(hash_file "$C_NEXT")" \
    "runtime_headers=$runtime_header_fingerprint" \
    "output=$OUTPUT_KEY" \
    "cc_profile=${PGY_SELFHOST_CC_PROFILE:-release}" \
    "cc_flags=${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[*]}" \
    "cc=$($CC --version 2>/dev/null | head -1)" \
    >"$KEY_INPUT"
build_key="$(hash_file "$KEY_INPUT")"

if [[ -x "$OUTPUT" && -f "$STAMP" ]] \
    && grep -Fxq "$build_key" "$STAMP" \
    && pgy_binary_is_runnable_here "$OUTPUT"; then
    rm -f "$C_NEXT"
    verify_installed_lsp_artifact "$OUTPUT" ||
        fail "fingerprinted Pergyra-built LSP failed artifact admission"
    echo "[self-host-lsp-build] reusing fingerprinted Pergyra-built LSP"
    exit 0
fi

mv -f "$C_NEXT" "$C_FILE"
tmp_output="${OUTPUT}.tmp"
rm -f "$tmp_output"
compile_command=("$CC" -x c -std=c11)
compile_command+=("${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}")
if pgy_selfhost_emitted_c_uses_runtime_headers "$C_FILE"; then
    compile_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
compile_command+=("$C_FILE" -o "$tmp_output")
if ! "${compile_command[@]}" >"$BUILD_DIR/lsp.compile.log" 2>&1; then
    tail -n 40 "$BUILD_DIR/lsp.compile.log" >&2 || true
    fail "emitted LSP C failed to compile"
fi

if ! verify_installed_lsp_artifact "$tmp_output"; then
    rm -f "$tmp_output"
    fail "new Pergyra-built LSP failed artifact admission"
fi

mv -f "$tmp_output" "$OUTPUT"
printf '%s\n' "$build_key" >"$STAMP"
echo "[self-host-lsp-build] Pergyra-built LSP installed: $OUTPUT"
