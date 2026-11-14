#!/usr/bin/env bash
# Sourced by installed_driver_cli_mode_owner.sh after its default MIR-C stdout,
# artifact, and pressure-observed evidence are ready.

cmp -s "$WORK_DIR/mir.stdout.normalized.c" \
    "$WORK_DIR/mir.artifact.normalized.c" ||
    fail "shared MIR-C admission changed stdout/artifact payload parity"
cmp -s "$WORK_DIR/mir.stdout.normalized.c" \
    "$WORK_DIR/mir.observed.normalized.c" ||
    fail "shared MIR-C admission changed observation payload parity"

MIR_C_MANIFEST="$(pgy_self_driver_machine_manifest_path "$DRIVER")"
[[ -f "$MIR_C_MANIFEST" ]] ||
    fail "missing installed machine declaration: $MIR_C_MANIFEST"
MIR_C_MANIFEST_REL="${MIR_C_MANIFEST#"$ROOT_DIR"/}"
[[ "$MIR_C_MANIFEST_REL" != "$MIR_C_MANIFEST" ]] ||
    fail "machine declaration escaped repository"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/source.mir.json" \
    --machine-manifest-json "$MIR_C_MANIFEST_REL") \
    >"$WORK_DIR/mir-manifest.c" 2>"$WORK_DIR/mir-manifest.err" ||
    fail "admitted-manifest MIR-C stdout failed"
grep -Fq '#include' "$WORK_DIR/mir-manifest.c" ||
    fail "admitted-manifest MIR-C stdout emitted no C payload"
! grep -Fq 'pgy_machine_layer_require_mapping();' "$WORK_DIR/mir-manifest.c" ||
    fail "non-machine MIR emitted a machine startup call"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    tests/cases/backend_compare/device_slot_machine_layer/main.pgy \
    --machine-manifest-json "$MIR_C_MANIFEST_REL") >"$WORK_DIR/machine.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/machine.mir.json" \
    --machine-manifest-json "$MIR_C_MANIFEST_REL") >"$WORK_DIR/machine.c"
grep -Fq 'pgy_machine_layer_require_mapping();' "$WORK_DIR/machine.c" ||
    fail "DeviceSlot MIR-C stdout lost its declaration mapping"

printf '%s\n' '{"schema":"invalid"}' >"$WORK_DIR/mir-invalid-manifest.json"
set +e
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/source.mir.json" \
    --machine-manifest-json "$WORK_REL/mir-invalid-manifest.json") \
    >"$WORK_DIR/mir-invalid-manifest.out" \
    2>"$WORK_DIR/mir-invalid-manifest.err"
mir_c_invalid_manifest_rc=$?
set -e
[[ "$mir_c_invalid_manifest_rc" -ne 0 ]] ||
    fail "invalid MIR-C machine declaration was accepted"
! grep -Fq '#include' "$WORK_DIR/mir-invalid-manifest.out" ||
    fail "invalid MIR-C machine declaration emitted a C payload"
grep -Fq 'MIR C machine declaration is invalid' \
    "$WORK_DIR/mir-invalid-manifest.out" \
    "$WORK_DIR/mir-invalid-manifest.err" ||
    fail "invalid MIR-C machine declaration lost its typed diagnostic"

echo "[driver-mir-c-stdout-action] shared payload admission, manifest identity, parity, and typed invalid-manifest rejection: PASS"
