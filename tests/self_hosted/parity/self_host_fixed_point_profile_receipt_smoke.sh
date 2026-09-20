#!/usr/bin/env bash
set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 || ! command -v mktemp >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/self_hosted/parity/self_host_fixed_point_profile_receipt_owner.sh"

CC_BIN="${CC:-gcc}"
command -v "$CC_BIN" >/dev/null 2>&1 || {
    echo "[self-host-fixed-point-profile-smoke] missing C compiler: $CC_BIN" >&2
    exit 1
}
RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-fixed-point-profile.XXXXXX")"
trap 'rm -rf "$RUN_DIR"' EXIT
RECEIPT="$RUN_DIR/profile.receipt"
FIXED="$RUN_DIR/fixed.receipt"

printf 'native\n' >"$RUN_DIR/native-pgy"
printf 'codegen\n' >"$RUN_DIR/codegen-seed"
printf 'func Main() -> Int { return 0; }\n' >"$RUN_DIR/driver.pgy"
printf 'seed-c\n' >"$RUN_DIR/driver-seed.c"
printf 'oracle-c\n' >"$RUN_DIR/driver-oracle.c"
printf '{"mir":1}\n' >"$RUN_DIR/seed.mir.json"
cp "$RUN_DIR/seed.mir.json" "$RUN_DIR/oracle.mir.json"
printf 'fixed-c\n' >"$RUN_DIR/gen2.c"
cp "$RUN_DIR/gen2.c" "$RUN_DIR/gen3.c"
printf 'binary\n' >"$RUN_DIR/gen2.bin"
printf '%s\n' \
    'schema=pgy.selfhost.driver-fixed-point-receipt.v1' \
    'source_graph=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef' \
    >"$FIXED"
pgy_selfhost_fixed_point_profile_begin "$RUN_DIR" "$RECEIPT"
# The smoke tests receipt shape and mutation rejection; the exact CI fixed
# point separately requires GNU time and proves the resource-metric path.
PGY_SELFHOST_PROFILE_TIME_BIN=""
PGY_SELFHOST_PROFILE_RESOURCE_SOURCE="shell-clock-only"
while IFS= read -r phase; do
    case "$phase" in
        full_mir_seed|full_mir_oracle)
            pgy_selfhost_fixed_point_profile_run_to_files \
                "$ROOT_DIR" "$phase" synthetic \
                "$RUN_DIR/${phase}.out" "$RUN_DIR/${phase}.err" \
                printf '%s\n' \
                '[driver-pressure-stage] ast:start' \
                '[driver-pressure-stage] ast:done' \
                '[driver-pressure-stage] json-write:start' \
                '[driver-pressure-stage] json-write:done' >/dev/null
            ;;
        *) pgy_selfhost_fixed_point_profile_run "$phase" synthetic true ;;
    esac
done < <(pgy_selfhost_fixed_point_profile_required_phases)
pgy_selfhost_fixed_point_profile_finish \
    "$ROOT_DIR" "$RUN_DIR/native-pgy" "$RUN_DIR/codegen-seed" "$CC_BIN" \
    "$RUN_DIR/driver.pgy" "$FIXED" "$RUN_DIR/driver-seed.c" \
    "$RUN_DIR/driver-oracle.c" "$RUN_DIR/seed.mir.json" \
    "$RUN_DIR/oracle.mir.json" "$RUN_DIR/gen2.c" "$RUN_DIR/gen3.c" \
    "$RUN_DIR/gen2.bin" >/dev/null
grep -Fxq 'schema=pgy.selfhost.fixed-point-profile.v1' "$RECEIPT"
grep -Eq '^dominant_phase=[a-z0-9_]+$' "$RECEIPT"
grep -Eq '^artifact=mir_seed\|bytes=[0-9]+\|lines=[0-9]+\|sha256=[0-9a-f]{64}$' "$RECEIPT"

cp "$RECEIPT" "$RUN_DIR/pristine.receipt"
printf 'drift\n' >>"$RUN_DIR/gen3.c"
if pgy_selfhost_fixed_point_profile_validate \
    "$RECEIPT" "$FIXED" "$RUN_DIR/driver.pgy" "$RUN_DIR/driver-seed.c" \
    "$RUN_DIR/driver-oracle.c" "$RUN_DIR/seed.mir.json" \
    "$RUN_DIR/oracle.mir.json" "$RUN_DIR/gen2.c" "$RUN_DIR/gen3.c" \
    "$RUN_DIR/gen2.bin" "$RUN_DIR/native-pgy" "$RUN_DIR/codegen-seed"; then
    echo "[self-host-fixed-point-profile-smoke] changed artifact was accepted" >&2
    exit 1
fi
cp "$RUN_DIR/gen2.c" "$RUN_DIR/gen3.c"
grep -v '^phase=gen3_emit|' "$RUN_DIR/pristine.receipt" >"$RECEIPT"
if pgy_selfhost_fixed_point_profile_validate \
    "$RECEIPT" "$FIXED" "$RUN_DIR/driver.pgy" "$RUN_DIR/driver-seed.c" \
    "$RUN_DIR/driver-oracle.c" "$RUN_DIR/seed.mir.json" \
    "$RUN_DIR/oracle.mir.json" "$RUN_DIR/gen2.c" "$RUN_DIR/gen3.c" \
    "$RUN_DIR/gen2.bin" "$RUN_DIR/native-pgy" "$RUN_DIR/codegen-seed"; then
    echo "[self-host-fixed-point-profile-smoke] missing phase was accepted" >&2
    exit 1
fi

awk '
    /^phase=/ && !changed { sub(/wall_ms=[0-9]+/, "wall_ms=bad"); changed=1 }
    { print }
' "$RUN_DIR/pristine.receipt" >"$RECEIPT"
if pgy_selfhost_fixed_point_profile_validate \
    "$RECEIPT" "$FIXED" "$RUN_DIR/driver.pgy" "$RUN_DIR/driver-seed.c" \
    "$RUN_DIR/driver-oracle.c" "$RUN_DIR/seed.mir.json" \
    "$RUN_DIR/oracle.mir.json" "$RUN_DIR/gen2.c" "$RUN_DIR/gen3.c" \
    "$RUN_DIR/gen2.bin" "$RUN_DIR/native-pgy" "$RUN_DIR/codegen-seed"; then
    echo "[self-host-fixed-point-profile-smoke] malformed timing was accepted" >&2
    exit 1
fi

echo "[self-host-fixed-point-profile-smoke] exact identity, phase census, and three negative mutations: PASS"
