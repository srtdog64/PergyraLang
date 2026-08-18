#!/usr/bin/env bash
# Builds the bounded DRV-2 compiler through the Pergyra typed-source codegen
# seed. The native compiler may build stage-0 seeds, but it must not compile
# the replacement driver source directly.

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
NATIVE_PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
CODEGEN_BUILD="${PGY_SELFHOST_CODEGEN_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/codegen/bootstrap}"
BUILD_DIR="${PGY_SELFHOST_COMPILER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/compiler/bootstrap}"
CODEGEN_BIN="${PGY_SELFHOST_CODEGEN_SEED:-$CODEGEN_BUILD/gen2.exe}"
DRIVER_SOURCE="src/self_hosted/compiler/driver_bootstrap_main.pgy"
OUTPUT="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
C_FILE="$BUILD_DIR/driver.c"
C_RAW="$BUILD_DIR/driver.c.raw"
C_NEXT="$BUILD_DIR/driver.c.next"
STAMP="$BUILD_DIR/driver.build.key"
KEY_INPUT="$BUILD_DIR/driver.build.key.input"
RUNTIME_HEADER_KEY_INPUT="$BUILD_DIR/driver.runtime-headers.build.key.input"
SMOKE_OUT="$BUILD_DIR/driver.smoke.c"
MANIFEST_SOURCE="$BUILD_DIR/machine-layer-manifest.json"
MANIFEST_SMOKE="$BUILD_DIR/machine-layer-manifest.smoke.json"

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
        if pgy_binary_expects_windows_paths "$CODEGEN_BIN"; then
            OUTPUT="${OUTPUT}.exe"
        fi
        ;;
esac
case "$OUTPUT" in
    *.exe) MANIFEST_OUTPUT="${OUTPUT%.exe}.machine-layer-manifest.json" ;;
    *) MANIFEST_OUTPUT="${OUTPUT}.machine-layer-manifest.json" ;;
esac
NATIVE_PGY="$(pgy_select_optional_exe_binary "$NATIVE_PGY")"

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
[[ -f "$CODEGEN_BIN" ]] || fail "missing Pergyra codegen seed: $CODEGEN_BIN"
[[ -f "$NATIVE_PGY" ]] || fail "missing native machine manifest owner: $NATIVE_PGY"
pgy_require_runnable_binary_here "self-host-compiler-build" "$CODEGEN_BIN" || exit 1
pgy_require_runnable_binary_here "self-host-compiler-build" "$NATIVE_PGY" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
pgy_selfhost_select_emitted_c_compile_profile ||
    fail self-host-emitted-c-profile-invalid

# The native serializer remains the sole physical-declaration producer. The
# installed Pergyra driver only validates and replays this immutable companion.
rm -f "$MANIFEST_SOURCE"
if ! (cd "$ROOT_DIR" && "$NATIVE_PGY" --native-pipeline \
    --machine-manifest-json >"$MANIFEST_SOURCE"); then
    fail "native machine manifest owner failed"
fi
grep -Fq '"schema":"pgy.machine-layer.declaration.v1"' "$MANIFEST_SOURCE" ||
    fail "native machine manifest owner emitted an invalid artifact"

# The source pipeline owns declaration provenance. An AST-text detour loses
# that fact and must fail closed for compiler-internal builtins, so emit the
# current composed graph through the typed source-artifact route every time.
echo "[self-host-compiler-build] emitting DRV-2 from typed source artifact"
rm -f "$C_RAW" "$C_NEXT"
if ! (cd "$ROOT_DIR" && MSYS2_ARG_CONV_EXCL="$PGY_ARG_CONV_EXCL" \
    "$CODEGEN_BIN" --source "$DRIVER_SOURCE" >"$C_RAW"); then
    fail "Pergyra-built codegen rejected the DRV-2 source graph"
fi
if ! tr -d '\r' <"$C_RAW" >"$C_NEXT"; then
    fail "Pergyra-built codegen C normalization failed"
fi
rm -f "$C_RAW"
if grep -q '^CODEGEN ERROR' "$C_NEXT"; then
    grep '^CODEGEN ERROR' "$C_NEXT" | head -5 >&2
    fail "DRV-2 is outside the Pergyra codegen subset"
fi
[[ -s "$C_NEXT" ]] || fail "Pergyra-built codegen emitted empty C"

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
    "schema=pgy.selfhost.compiler-build.v4-source-artifact" \
    "source_artifact_c=$(hash_file "$C_NEXT")" \
    "machine_manifest=$(hash_file "$MANIFEST_SOURCE")" \
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
    if [[ ! -f "$MANIFEST_OUTPUT" ]] ||
        ! cmp -s "$MANIFEST_SOURCE" "$MANIFEST_OUTPUT"; then
        cp "$MANIFEST_SOURCE" "${MANIFEST_OUTPUT}.tmp"
        mv -f "${MANIFEST_OUTPUT}.tmp" "$MANIFEST_OUTPUT"
    fi
    rm -f "$C_NEXT"
    echo "[self-host-compiler-build] reusing fingerprinted Pergyra-built driver"
    exit 0
fi

mv -f "$C_NEXT" "$C_FILE"

tmp_output="${OUTPUT}.tmp"
rm -f "$tmp_output"
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

smoke_rel="${SMOKE_OUT#"$ROOT_DIR"/}"
rm -f "$SMOKE_OUT"
if ! (cd "$ROOT_DIR" && MSYS2_ARG_CONV_EXCL="$PGY_ARG_CONV_EXCL" "$tmp_output" \
    --emit-c-artifact-verified \
    src/self_hosted/semantic/fixture/valid_call_int.pgy "$smoke_rel"); then
    rm -f "$tmp_output"
    fail "Pergyra-built DRV-2 failed its bounded source smoke"
fi
if [[ ! -s "$SMOKE_OUT" ]]; then
    rm -f "$tmp_output"
    fail "Pergyra-built DRV-2 emitted no smoke artifact"
fi
rm -f "$MANIFEST_SMOKE"
manifest_rel="${MANIFEST_SOURCE#"$ROOT_DIR"/}"
if ! (cd "$ROOT_DIR" && MSYS2_ARG_CONV_EXCL="$PGY_ARG_CONV_EXCL" \
    "$tmp_output" --emit-machine-manifest-verified "$manifest_rel" \
    >"$MANIFEST_SMOKE"); then
    rm -f "$tmp_output"
    fail "Pergyra-built DRV-2 rejected the installed machine manifest"
fi
# Hash equality instead of cmp: the CI-provisioned MSYS2 ships without
# diffutils, and a `cmp: command not found` (exit 127) read as "artifact
# changed" — a missing tool must not impersonate a manifest verdict.
if [[ "$(hash_file "$MANIFEST_SOURCE")" != "$(hash_file "$MANIFEST_SMOKE")" ]]; then
    rm -f "$tmp_output"
    fail "Pergyra-built DRV-2 changed the native machine manifest artifact"
fi
cp "$MANIFEST_SOURCE" "${MANIFEST_OUTPUT}.tmp"
mv -f "${MANIFEST_OUTPUT}.tmp" "$MANIFEST_OUTPUT"
mv -f "$tmp_output" "$OUTPUT"
printf '%s\n' "$build_key" >"$STAMP"
echo "[self-host-compiler-build] Pergyra-built DRV-2 installed: $OUTPUT"
