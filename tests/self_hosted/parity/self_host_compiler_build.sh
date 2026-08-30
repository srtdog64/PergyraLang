#!/usr/bin/env bash
# Builds the bounded DRV-2 compiler through the Pergyra typed-source codegen
# seed. The native compiler may build stage-0 seeds, but it must not compile
# the replacement driver source directly.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/self_host_driver_fixed_point_receipt_owner.sh"
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
PREBUILD_STAMP="$BUILD_DIR/driver.source-graph.build.key"
PREBUILD_KEY_INPUT="$BUILD_DIR/driver.source-graph.build.key.input"
SOURCE_GRAPH_KEY_INPUT="$BUILD_DIR/driver.source-graph.input"
OUTPUT_RECEIPT="$BUILD_DIR/driver.output.receipt"
CC_FINGERPRINT_KEY_INPUT="$BUILD_DIR/driver.cc-fingerprint.input"
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

hash_file() { pgy_selfhost_driver_receipt_hash_file "$1"; }

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

runtime_header_fingerprint="$(pgy_selfhost_driver_runtime_header_fingerprint "$ROOT_DIR" "$RUNTIME_HEADER_KEY_INPUT")" || fail "runtime-header fingerprint failed"
cc_fingerprint="$(pgy_selfhost_driver_c_compiler_fingerprint "$CC" "$CC_FINGERPRINT_KEY_INPUT")" || fail "C compiler fingerprint failed"
prebuild_key="$(pgy_selfhost_driver_installer_prebuild_key "$ROOT_DIR" "$CODEGEN_BIN" "$MANIFEST_SOURCE" "$runtime_header_fingerprint" "$OUTPUT_KEY" "${PGY_SELFHOST_CC_PROFILE:-release}" "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[*]}" "$cc_fingerprint" "${BASH_SOURCE[0]}" "$PREBUILD_KEY_INPUT" "$SOURCE_GRAPH_KEY_INPUT")" || fail "source-graph build key failed"

FIXED_POINT_C="${PGY_SELFHOST_FIXED_POINT_DRIVER_C:-}"
FIXED_POINT_GEN3_C="${PGY_SELFHOST_FIXED_POINT_DRIVER_GEN3_C:-}"
FIXED_POINT_BIN="${PGY_SELFHOST_FIXED_POINT_DRIVER_BIN:-}"
FIXED_POINT_RECEIPT="${PGY_SELFHOST_FIXED_POINT_DRIVER_RECEIPT:-}"
candidate_output=""
if [[ -n "${FIXED_POINT_C}${FIXED_POINT_GEN3_C}${FIXED_POINT_BIN}${FIXED_POINT_RECEIPT}" ]]; then
    pgy_selfhost_driver_validate_fixed_point_receipt "$ROOT_DIR" "$CODEGEN_BIN" "$FIXED_POINT_C" "$FIXED_POINT_GEN3_C" "$FIXED_POINT_BIN" "$FIXED_POINT_RECEIPT" || fail "explicit fixed-point driver receipt was rejected"
    pgy_require_runnable_binary_here "self-host-compiler-build" "$FIXED_POINT_BIN" || fail "fixed-point driver is not runnable here"
    rm -f "$C_RAW" "$C_NEXT"
    cp "$FIXED_POINT_C" "$C_NEXT"
    candidate_output="$FIXED_POINT_BIN"
    echo "[self-host-compiler-build] adopting receipt-bound fixed-point driver"
elif [[ -x "$OUTPUT" && -f "$PREBUILD_STAMP" ]] \
    && grep -Fxq "$prebuild_key" "$PREBUILD_STAMP" \
    && pgy_selfhost_driver_validate_installed_artifact_receipt "$OUTPUT" "$OUTPUT_RECEIPT" \
    && pgy_binary_is_runnable_here "$OUTPUT"; then
    cp "$MANIFEST_SOURCE" "${MANIFEST_OUTPUT}.tmp"
    mv -f "${MANIFEST_OUTPUT}.tmp" "$MANIFEST_OUTPUT"
    echo "[self-host-compiler-build] reusing source-graph fingerprinted driver before emission"
    exit 0
else
    # The typed source artifact remains the ordinary producer. The prebuild
    # receipt only rejects an identical repeated compiler-scale operation.
    echo "[self-host-compiler-build] emitting DRV-2 from typed source artifact"
    rm -f "$C_RAW" "$C_NEXT"
    if ! (cd "$ROOT_DIR" && MSYS2_ARG_CONV_EXCL="$PGY_ARG_CONV_EXCL" \
        "$CODEGEN_BIN" --source "$DRIVER_SOURCE" >"$C_RAW"); then
        fail "Pergyra-built codegen rejected the DRV-2 source graph"
    fi
    tr -d '\r' <"$C_RAW" >"$C_NEXT" || fail "Pergyra-built codegen C normalization failed"
    rm -f "$C_RAW"
    if grep -q '^CODEGEN ERROR' "$C_NEXT"; then
        grep '^CODEGEN ERROR' "$C_NEXT" | head -5 >&2
        fail "DRV-2 is outside the Pergyra codegen subset"
    fi
    [[ -s "$C_NEXT" ]] || fail "Pergyra-built codegen emitted empty C"
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

mv -f "$C_NEXT" "$C_FILE"

tmp_output="${OUTPUT}.tmp"
rm -f "$tmp_output"
if [[ -n "$candidate_output" ]]; then
    cp "$candidate_output" "$tmp_output"
else
    compile_command=("$CC" -x c -std=c11)
    compile_command+=(${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]})
    pgy_selfhost_emitted_c_uses_runtime_headers "$C_FILE" && compile_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    compile_command+=("$C_FILE" -o "$tmp_output")
    if ! "${compile_command[@]}" >"$BUILD_DIR/driver.compile.log" 2>&1; then
        tail -n 40 "$BUILD_DIR/driver.compile.log" >&2 || true
        fail "emitted DRV-2 C failed to compile"
    fi
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
if [[ -n "$candidate_output" ]]; then
    rm -f "$STAMP" "$PREBUILD_STAMP" "$OUTPUT_RECEIPT"
else
    printf '%s\n' "$build_key" >"$STAMP"
    printf '%s\n' "$prebuild_key" >"$PREBUILD_STAMP"
    pgy_selfhost_driver_write_installed_artifact_receipt "$OUTPUT" "$OUTPUT_RECEIPT"
fi
echo "[self-host-compiler-build] Pergyra-built DRV-2 installed: $OUTPUT"
