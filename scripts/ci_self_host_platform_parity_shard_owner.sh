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
        exec "$BASH" "$ROOT_DIR/tests/self_hosted/parity/parser_parity.sh"
        ;;
    semantic)
        export PGY_SELFHOST_SEMANTIC_BACKENDS="$BACKENDS"
        exec "$BASH" "$ROOT_DIR/tests/self_hosted/parity/semantic_parity.sh"
        ;;
    codegen)
        export PGY_SELFHOST_CODEGEN_BACKENDS="$BACKENDS"
        exec "$BASH" "$ROOT_DIR/tests/self_hosted/parity/codegen_parity.sh"
        ;;
    driver)
        export PGY_SELFHOST_DRIVER_BACKENDS="$BACKENDS"
        exec "$BASH" "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_body_parity.sh"
        ;;
    *)
        fail "unknown shard; expected parser, semantic, codegen, or driver"
        ;;
esac
