#!/usr/bin/env bash
# Falsifiers for the DRV-2 source artifact admission that lets one platform's
# toolchain job compile the C another platform's job emitted.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/self_hosted/parity/driver/self_host_driver_source_artifact_receipt_owner.sh"

BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/driver/source-artifact-receipt-smoke"
CASE_ROOT="$BUILD_DIR/case-root"
GRAPH_DIR="$CASE_ROOT/src/self_hosted/compiler"
ENTRY="src/self_hosted/compiler/driver_main.pgy"
ARTIFACT_C="$BUILD_DIR/driver.c"
RECEIPT="$BUILD_DIR/driver.source-artifact.receipt"

fail() {
    echo "[self-host-driver-source-artifact-smoke] $*" >&2
    exit 1
}

write_clean_inputs() {
    printf 'func A() -> Int { return 1; }\n' >"$GRAPH_DIR/a.pgy"
    printf 'func Main() -> Int { return A(); }\n' >"$CASE_ROOT/$ENTRY"
    printf 'int pgy_driver(void) { return 1; }\n' >"$ARTIFACT_C"
}

admit() {
    pgy_selfhost_driver_source_artifact_admit "$CASE_ROOT" "$ENTRY" "$@" >/dev/null 2>&1
}

require_refused() {
    local label="$1"
    shift
    if admit "$@"; then
        fail "$label was admitted"
    fi
}

rm -rf "$BUILD_DIR"
mkdir -p "$GRAPH_DIR"
write_clean_inputs
pgy_selfhost_driver_write_source_artifact_receipt "$CASE_ROOT" "$ENTRY" "$ARTIFACT_C" "$RECEIPT"
admit adopt "$ARTIFACT_C" "$RECEIPT" || fail "exact source artifact was refused for adopt"
admit reemit "$ARTIFACT_C" "$RECEIPT" || fail "exact source artifact was refused for reemit"
admit "" "" "" || fail "an ordinary build without an artifact was refused"

# A host whose sha256sum marks binary reads ('*', as MSYS2 does) reads the
# same source graph.
SHIM_DIR="$BUILD_DIR/msys-sha256sum"
REAL_SHA256SUM="$(command -v sha256sum)"
mkdir -p "$SHIM_DIR"
printf '#!/usr/bin/env bash\n"%s" "$@" | sed -E "s/^([0-9a-f]{64})  /\\\\1 */"\n' \
    "$REAL_SHA256SUM" >"$SHIM_DIR/sha256sum"
chmod +x "$SHIM_DIR/sha256sum"
[[ "$(PATH="$SHIM_DIR:$PATH" sha256sum "$ARTIFACT_C" | cut -c65-66)" == " *" ]] ||
    fail "binary-mode sha256sum shim did not mark its output"
PATH="$SHIM_DIR:$PATH" admit adopt "$ARTIFACT_C" "$RECEIPT" ||
    fail "a binary-mode sha256sum host read another source graph"

printf 'int pgy_driver(void) { return 2; }\n' >"$ARTIFACT_C"
require_refused "changed DRV-2 C" adopt "$ARTIFACT_C" "$RECEIPT"
write_clean_inputs
printf 'func A() -> Int { return 2; }\n' >"$GRAPH_DIR/a.pgy"
require_refused "changed source graph" adopt "$ARTIFACT_C" "$RECEIPT"
write_clean_inputs
pgy_selfhost_driver_write_source_artifact_receipt "$CASE_ROOT" "$ENTRY" "$ARTIFACT_C" "$RECEIPT"
sed 's/^driver_source=.*/driver_source=src\/other_main.pgy/' "$RECEIPT" >"${RECEIPT}.mutated"
mv -f "${RECEIPT}.mutated" "$RECEIPT"
require_refused "another DRV-2 entry" adopt "$ARTIFACT_C" "$RECEIPT"
pgy_selfhost_driver_write_source_artifact_receipt "$CASE_ROOT" "$ENTRY" "$ARTIFACT_C" "$RECEIPT"
require_refused "an artifact without a mode" "" "$ARTIFACT_C" "$RECEIPT"
require_refused "an unknown mode" install "$ARTIFACT_C" "$RECEIPT"
require_refused "an artifact without its receipt" adopt "$ARTIFACT_C" "$BUILD_DIR/missing.receipt"
if PGY_SELFHOST_FIXED_POINT_DRIVER_C="$ARTIFACT_C" admit adopt "$ARTIFACT_C" "$RECEIPT"; then
    fail "a source artifact beside a fixed-point driver was admitted"
fi

cp "$ARTIFACT_C" "$BUILD_DIR/emitted.c"
pgy_selfhost_driver_source_artifact_require_reemission "$BUILD_DIR/emitted.c" \
    "$ARTIFACT_C" "$BUILD_DIR/kept.c" >/dev/null || fail "an identical re-emission was refused"
printf 'int pgy_driver(void) { return 3; }\n' >"$BUILD_DIR/emitted.c"
if pgy_selfhost_driver_source_artifact_require_reemission "$BUILD_DIR/emitted.c" \
    "$ARTIFACT_C" "$BUILD_DIR/kept.c" >/dev/null 2>&1; then
    fail "a different re-emission was admitted"
fi
[[ -f "$BUILD_DIR/kept.c" ]] || fail "a different re-emission was not kept"

echo "[self-host-driver-source-artifact-smoke] PASS"
