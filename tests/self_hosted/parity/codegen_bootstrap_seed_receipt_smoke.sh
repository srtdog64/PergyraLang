#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/codegen_bootstrap_seed_receipt_owner.sh"

BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/codegen-seed-receipt-smoke"
CASE_ROOT="$BUILD_DIR/case-root"
GRAPH_DIR="$CASE_ROOT/src/self_hosted/codegen"
SOURCE_ARTIFACT="$BUILD_DIR/gen2.c"
BINARY_ARTIFACT="$BUILD_DIR/gen2.exe"
RECEIPT="$BUILD_DIR/output.receipt"

fail() {
    echo "[self-host-codegen-seed-receipt-smoke] $*" >&2
    exit 1
}

write_clean_inputs() {
    printf 'func Main() -> Int { return 1; }\n' >"$GRAPH_DIR/main.pgy"
    printf 'native-pgy-v1\n' >"$BUILD_DIR/native-pgy"
    printf 'generated-seed-c-v1\n' >"$SOURCE_ARTIFACT"
    printf '#!/usr/bin/env bash\nexit 0\n' >"$BINARY_ARTIFACT"
    chmod +x "$BINARY_ARTIFACT"
}

seed_key() {
    local suffix="$1" cc_identity="$2"
    pgy_selfhost_codegen_seed_prebuild_key \
        "$CASE_ROOT" "$BUILD_DIR/native-pgy" "$cc_identity" runtime-v1 \
        release '-O3 -fwrapv' "$BUILD_DIR/owners/codegen_bootstrap.sh" \
        "$BUILD_DIR/key-${suffix}.input" "$BUILD_DIR/graph-${suffix}.input"
}

rm -rf "$BUILD_DIR"
mkdir -p "$GRAPH_DIR" "$BUILD_DIR/owners"
for owner in codegen_bootstrap.sh codegen_bootstrap_compile_leg.sh \
    parser_tool_build_leg.sh emitted_c_runtime_header_owner.sh; do
    printf '%s-v1\n' "$owner" >"$BUILD_DIR/owners/$owner"
done
mkdir -p "$CASE_ROOT/tests"
printf 'path-owner-v1\n' >"$CASE_ROOT/tests/pgy_binary_path_helpers.sh"
write_clean_inputs

pgy_selfhost_codegen_seed_write_artifact_receipt \
    "$SOURCE_ARTIFACT" "$BINARY_ARTIFACT" "$RECEIPT"
pgy_selfhost_codegen_seed_validate_artifact_receipt \
    "$SOURCE_ARTIFACT" "$BINARY_ARTIFACT" "$RECEIPT" ||
    fail "exact artifact receipt was rejected"

key_one="$(seed_key one cc-v1)"
key_two="$(seed_key two cc-v1)"
[[ "$key_one" == "$key_two" ]] || fail "identical inputs changed prebuild key"

printf 'func Main() -> Int { return 2; }\n' >"$GRAPH_DIR/main.pgy"
key_source="$(seed_key source cc-v1)"
[[ "$key_one" != "$key_source" ]] || fail "source mutation kept prebuild key"
write_clean_inputs
key_cc="$(seed_key cc cc-v2)"
[[ "$key_one" != "$key_cc" ]] || fail "compiler mutation kept prebuild key"
printf 'codegen-bootstrap-v2\n' >"$BUILD_DIR/owners/codegen_bootstrap.sh"
key_owner="$(seed_key owner cc-v1)"
[[ "$key_one" != "$key_owner" ]] || fail "owner mutation kept prebuild key"

printf 'changed-seed-c\n' >"$SOURCE_ARTIFACT"
if pgy_selfhost_codegen_seed_validate_artifact_receipt \
    "$SOURCE_ARTIFACT" "$BINARY_ARTIFACT" "$RECEIPT"; then
    fail "changed C artifact was admitted"
fi
write_clean_inputs
pgy_selfhost_codegen_seed_write_artifact_receipt \
    "$SOURCE_ARTIFACT" "$BINARY_ARTIFACT" "$RECEIPT"
printf '#!/usr/bin/env bash\nexit 1\n' >"$BINARY_ARTIFACT"
chmod +x "$BINARY_ARTIFACT"
if pgy_selfhost_codegen_seed_validate_artifact_receipt \
    "$SOURCE_ARTIFACT" "$BINARY_ARTIFACT" "$RECEIPT"; then
    fail "changed binary artifact was admitted"
fi
write_clean_inputs
pgy_selfhost_codegen_seed_write_artifact_receipt \
    "$SOURCE_ARTIFACT" "$BINARY_ARTIFACT" "$RECEIPT"
sed 's/artifact.v1/artifact.v2/' "$RECEIPT" >"${RECEIPT}.mutated"
mv -f "${RECEIPT}.mutated" "$RECEIPT"
if pgy_selfhost_codegen_seed_validate_artifact_receipt \
    "$SOURCE_ARTIFACT" "$BINARY_ARTIFACT" "$RECEIPT"; then
    fail "changed receipt schema was admitted"
fi

echo "[self-host-codegen-seed-receipt-smoke] PASS"
