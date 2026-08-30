#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/self_hosted/parity/self_host_driver_fixed_point_receipt_owner.sh"

BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/driver/fixed-point-receipt-smoke"
CASE_ROOT="$BUILD_DIR/case-root"
GRAPH_DIR="$CASE_ROOT/src/self_hosted/compiler"
CODEGEN="$BUILD_DIR/codegen-seed"
GEN2_C="$BUILD_DIR/driver-gen2.c"
GEN3_C="$BUILD_DIR/driver-gen3.c"
GEN2_BIN="$BUILD_DIR/driver-gen2.bin"
RECEIPT="$BUILD_DIR/fixed-point.receipt"

fail() {
    echo "[self-host-driver-fixed-point-receipt-smoke] $*" >&2
    exit 1
}

write_clean_inputs() {
    printf 'func A() -> Int { return 1; }\n' >"$GRAPH_DIR/a.pgy"
    printf 'func B() -> Int { return A(); }\n' >"$GRAPH_DIR/b.pgy"
    printf 'codegen-seed-v1\n' >"$CODEGEN"
    printf 'generated-driver-c-v1\n' >"$GEN2_C"
    printf 'generated-driver-c-v1\n' >"$GEN3_C"
    printf '#!/usr/bin/env bash\nexit 0\n' >"$GEN2_BIN"
    chmod +x "$GEN2_BIN"
}

require_rejected() {
    local label="$1"
    if pgy_selfhost_driver_validate_fixed_point_receipt \
        "$CASE_ROOT" "$CODEGEN" "$GEN2_C" "$GEN3_C" "$GEN2_BIN" "$RECEIPT" \
        >/dev/null 2>&1; then
        fail "$label mutation was admitted"
    fi
}

rm -rf "$BUILD_DIR"
mkdir -p "$GRAPH_DIR"
write_clean_inputs
pgy_selfhost_driver_write_fixed_point_receipt \
    "$CASE_ROOT" "$CODEGEN" "$GEN2_C" "$GEN3_C" "$GEN2_BIN" "$RECEIPT"
pgy_selfhost_driver_validate_fixed_point_receipt \
    "$CASE_ROOT" "$CODEGEN" "$GEN2_C" "$GEN3_C" "$GEN2_BIN" "$RECEIPT" ||
    fail "exact fixed-point receipt was rejected"

printf 'func A() -> Int { return 2; }\n' >"$GRAPH_DIR/a.pgy"
require_rejected source-graph
write_clean_inputs
printf 'codegen-seed-v2\n' >"$CODEGEN"
require_rejected codegen-seed
write_clean_inputs
printf 'changed-gen2\n' >"$GEN2_C"
require_rejected gen2-c
write_clean_inputs
printf 'changed-gen3\n' >"$GEN3_C"
require_rejected gen3-c
write_clean_inputs
printf '#!/usr/bin/env bash\nexit 1\n' >"$GEN2_BIN"
chmod +x "$GEN2_BIN"
require_rejected gen2-binary

write_clean_inputs
pgy_selfhost_driver_write_fixed_point_receipt \
    "$CASE_ROOT" "$CODEGEN" "$GEN2_C" "$GEN3_C" "$GEN2_BIN" "$RECEIPT"
sed 's/receipt.v1/receipt.v2/' "$RECEIPT" >"${RECEIPT}.mutated"
mv -f "${RECEIPT}.mutated" "$RECEIPT"
require_rejected schema
pgy_selfhost_driver_write_fixed_point_receipt \
    "$CASE_ROOT" "$CODEGEN" "$GEN2_C" "$GEN3_C" "$GEN2_BIN" "$RECEIPT"
grep -v '^gen3_c=' "$RECEIPT" >"${RECEIPT}.mutated"
mv -f "${RECEIPT}.mutated" "$RECEIPT"
require_rejected missing-field
pgy_selfhost_driver_write_fixed_point_receipt \
    "$CASE_ROOT" "$CODEGEN" "$GEN2_C" "$GEN3_C" "$GEN2_BIN" "$RECEIPT"
printf 'gen2_c=duplicate\n' >>"$RECEIPT"
require_rejected duplicate-field

ARTIFACT_RECEIPT="$BUILD_DIR/installed-artifact.receipt"
pgy_selfhost_driver_write_installed_artifact_receipt "$GEN2_BIN" "$ARTIFACT_RECEIPT"
pgy_selfhost_driver_validate_installed_artifact_receipt \
    "$GEN2_BIN" "$ARTIFACT_RECEIPT" || fail "installed artifact receipt rejected"
printf '#!/usr/bin/env bash\nexit 2\n' >"$GEN2_BIN"
chmod +x "$GEN2_BIN"
if pgy_selfhost_driver_validate_installed_artifact_receipt \
    "$GEN2_BIN" "$ARTIFACT_RECEIPT"; then
    fail "changed installed artifact was admitted"
fi

write_clean_inputs
printf '{"schema":"test"}\n' >"$BUILD_DIR/manifest.json"
printf 'installer-owner-v1\n' >"$BUILD_DIR/installer.sh"
key_one="$(pgy_selfhost_driver_installer_prebuild_key "$CASE_ROOT" "$CODEGEN" "$BUILD_DIR/manifest.json" runtime-v1 output-v1 release '-O3 -fwrapv' cc-v1 "$BUILD_DIR/installer.sh" "$BUILD_DIR/key-one.input" "$BUILD_DIR/graph-one.input")"
key_two="$(pgy_selfhost_driver_installer_prebuild_key "$CASE_ROOT" "$CODEGEN" "$BUILD_DIR/manifest.json" runtime-v1 output-v1 release '-O3 -fwrapv' cc-v1 "$BUILD_DIR/installer.sh" "$BUILD_DIR/key-two.input" "$BUILD_DIR/graph-two.input")"
[[ "$key_one" == "$key_two" ]] || fail "identical prebuild inputs changed key"
printf 'func A() -> Int { return 3; }\n' >"$GRAPH_DIR/a.pgy"
key_three="$(pgy_selfhost_driver_installer_prebuild_key "$CASE_ROOT" "$CODEGEN" "$BUILD_DIR/manifest.json" runtime-v1 output-v1 release '-O3 -fwrapv' cc-v1 "$BUILD_DIR/installer.sh" "$BUILD_DIR/key-three.input" "$BUILD_DIR/graph-three.input")"
[[ "$key_one" != "$key_three" ]] || fail "source mutation did not change prebuild key"

echo "[self-host-driver-fixed-point-receipt-smoke] PASS"
