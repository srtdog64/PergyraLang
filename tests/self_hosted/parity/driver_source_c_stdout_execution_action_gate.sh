#!/usr/bin/env bash
# Sourced by driver_source_c_execution_action_gate.sh after its installed
# driver and artifact evidence are ready.

(cd "$ROOT_DIR" && "$SELF_DRIVER" "$SOURCE") >"$WORK_DIR/stdout-default.c"
(cd "$ROOT_DIR" && "$SELF_DRIVER" "$SOURCE" --emit-c-verified) \
    >"$WORK_DIR/stdout-explicit.c"
tr -d '\r' <"$WORK_DIR/stdout-default.c" >"$WORK_DIR/stdout-default.normalized.c"
tr -d '\r' <"$WORK_DIR/direct.c" >"$WORK_DIR/direct.normalized.c"
cmp -s "$WORK_DIR/stdout-default.c" "$WORK_DIR/stdout-explicit.c" ||
    fail "default and explicit source-C stdout bytes differ"
cmp -s "$WORK_DIR/stdout-default.normalized.c" "$WORK_DIR/direct.normalized.c" ||
    fail "source-C stdout and artifact actions produced different payloads"

MANIFEST="$(pgy_self_driver_machine_manifest_path "$SELF_DRIVER")"
[[ -f "$MANIFEST" ]] || fail "missing installed machine declaration: $MANIFEST"
MANIFEST_REL="${MANIFEST#"$ROOT_DIR"/}"
[[ "$MANIFEST_REL" != "$MANIFEST" ]] || fail "machine declaration escaped repository"
(cd "$ROOT_DIR" && "$SELF_DRIVER" "$SOURCE" --machine-manifest-json "$MANIFEST_REL") \
    >"$WORK_DIR/stdout-non-machine-manifest.c"
! grep -Fq 'pgy_machine_layer_require_mapping();' "$WORK_DIR/stdout-non-machine-manifest.c" ||
    fail "non-machine source emitted a machine startup call"
(cd "$ROOT_DIR" && "$SELF_DRIVER" tests/cases/backend_compare/device_slot_machine_layer/main.pgy \
    --machine-manifest-json "$MANIFEST_REL") \
    >"$WORK_DIR/stdout-manifest.c"
grep -Fq '#include' "$WORK_DIR/stdout-manifest.c" ||
    fail "manifest source-C stdout emitted no C payload"
grep -Fq 'pgy_machine_layer_require_mapping();' "$WORK_DIR/stdout-manifest.c" ||
    fail "DeviceSlot source-C stdout lost its admitted machine declaration"

printf '%s\n' '{"schema":"invalid"}' >"$WORK_DIR/invalid-manifest.json"
set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" "$SOURCE" --machine-manifest-json \
    "$WORK_REL/invalid-manifest.json") >"$WORK_DIR/invalid-manifest.out" \
    2>"$WORK_DIR/invalid-manifest.err"
invalid_manifest_rc=$?
set -e
[[ "$invalid_manifest_rc" -ne 0 ]] || fail "invalid machine declaration was accepted"
! grep -Fq '#include' "$WORK_DIR/invalid-manifest.out" ||
    fail "invalid machine declaration emitted a C payload"
grep -Fq 'source C machine declaration is invalid' \
    "$WORK_DIR/invalid-manifest.out" "$WORK_DIR/invalid-manifest.err" ||
    fail "invalid machine declaration lost its typed diagnostic"

echo "[driver-source-c-stdout-action] default/explicit raw byte parity, normalized artifact payload parity, admitted manifest, and typed invalid-manifest rejection: PASS"
