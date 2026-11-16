#!/usr/bin/env bash
# Runs exactly one platform self-host parity owner against a same-run installed
# toolchain artifact. This boundary deliberately has no build fallback: a
# missing or mismatched artifact invalidates the shard instead of changing its
# proof input.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

SHARD="${PGY_CI_SELF_HOST_PARITY_SHARD:-}"
BACKENDS="${PGY_CI_SELF_HOST_PARITY_BACKENDS:-}"
PGY="${PGY_BIN:-}"
DRIVER="${PGY_SELF_DRIVER_BIN:-}"

fail() {
    echo "[ci-self-host-platform-parity:$SHARD] $*" >&2
    exit 1
}

[[ -n "$SHARD" ]] || fail "missing PGY_CI_SELF_HOST_PARITY_SHARD"
[[ -n "$BACKENDS" ]] || fail "missing PGY_CI_SELF_HOST_PARITY_BACKENDS"
[[ -n "$PGY" ]] || fail "missing PGY_BIN"
[[ -n "$DRIVER" ]] || fail "missing PGY_SELF_DRIVER_BIN"

PGY="$(pgy_select_optional_exe_binary "$PGY")"
DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
case "$DRIVER" in
    *.exe) MANIFEST="${DRIVER%.exe}.machine-layer-manifest.json" ;;
    *) MANIFEST="${DRIVER}.machine-layer-manifest.json" ;;
esac

[[ -x "$PGY" ]] || fail "installed compiler is not executable: $PGY"
[[ -x "$DRIVER" ]] || fail "installed self-host driver is not executable: $DRIVER"
[[ -s "$MANIFEST" ]] || fail "installed machine-layer manifest is missing: $MANIFEST"
grep -Fq '"schema":"pgy.machine-layer.declaration.v1"' "$MANIFEST" ||
    fail "installed machine-layer manifest schema is invalid"

export PGY_BIN="$PGY"
export PGY_SELF_DRIVER_BIN="$DRIVER"

case "$SHARD" in
    parser)
        export PGY_SELFHOST_PARSER_BACKENDS="$BACKENDS"
        exec "$BASH" "$ROOT_DIR/tests/self_hosted/parity/frontend/parser_parity.sh"
        ;;
    semantic)
        export PGY_SELFHOST_SEMANTIC_BACKENDS="$BACKENDS"
        exec "$BASH" "$ROOT_DIR/tests/self_hosted/parity/semantic/semantic_parity.sh"
        ;;
    codegen)
        export PGY_SELFHOST_CODEGEN_BACKENDS="$BACKENDS"
        exec "$BASH" "$ROOT_DIR/tests/self_hosted/parity/codegen/codegen_parity.sh"
        ;;
    driver)
        export PGY_SELFHOST_DRIVER_BACKENDS="$BACKENDS"
        # The C lane runs the installed driver. Rebuilding the DRV-2 driver
        # from source here would replace the proof input, and that compile
        # needs more memory than a hosted runner has.
        export PGY_SELFHOST_PREBUILT_DRIVER="$DRIVER"
        exec "$BASH" "$ROOT_DIR/tests/self_hosted/parity/driver/driver_rung2_body_parity.sh"
        ;;
    codegen-seed)
        # The installed driver of this platform is the DRV-2 C another
        # platform's toolchain job emitted, compiled here. This shard builds
        # this platform's codegen seed from the installed native compiler with
        # the seed's own gates, then requires that seed to re-emit the same
        # DRV-2 C byte for byte. Nothing is installed or replaced.
        ARTIFACT_DIR="${PGY_CI_DRV2_SOURCE_ARTIFACT_DIR:-$ROOT_DIR/.tmp/self_hosted/compiler/admitted}"
        [[ -s "$ARTIFACT_DIR/driver.c" && -s "$ARTIFACT_DIR/driver.source-artifact.receipt" ]] ||
            fail "the DRV-2 source artifact is missing: $ARTIFACT_DIR"
        PGY_SELFHOST_CODEGEN_SEED_ONLY=1 PGY_BIN="$PGY" \
            "$BASH" "$ROOT_DIR/tests/self_hosted/parity/codegen/codegen_bootstrap.sh"
        export PGY_SELFHOST_DRIVER_SOURCE_ARTIFACT_MODE=reemit
        export PGY_SELFHOST_DRIVER_SOURCE_ARTIFACT_C="$ARTIFACT_DIR/driver.c"
        export PGY_SELFHOST_DRIVER_SOURCE_ARTIFACT_RECEIPT="$ARTIFACT_DIR/driver.source-artifact.receipt"
        export PGY_SELFHOST_COMPILER_BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/compiler/reemit"
        export PGY_SELF_DRIVER_BIN="$PGY_SELFHOST_COMPILER_BUILD_DIR/unused-pgy-self-driver"
        exec "$BASH" "$ROOT_DIR/tests/self_hosted/parity/driver/self_host_compiler_build.sh"
        ;;
    harness-evidence)
        command -v sha256sum >/dev/null || fail "SHA-256 tool is required for installed harness provenance"
        printf '[platform-full-harness-evidence] revision=%s\n' "$(git -C "$ROOT_DIR" rev-parse HEAD)"
        sha256sum "$PGY" "$DRIVER" "$MANIFEST"
        export PGY_CI_NAME=platform-full-harness-evidence
        exec "$BASH" "$ROOT_DIR/scripts/ci_step_runner.sh" \
            "$ROOT_DIR/scripts/ci_test_harness_evidence_steps.sh"
        ;;
    *)
        fail "unknown shard; expected parser, semantic, codegen, driver, codegen-seed, or harness-evidence"
        ;;
esac
