#!/usr/bin/env bash
# Current native semantic promises and the installed capability-manifest owner.
# General installed source-MIR gaps are audit findings, never expected success.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_DIR="${PGY_CONCEPT_AUTHORITY_EFFECT_WORK_DIR:-$ROOT_DIR/.tmp/self_hosted/concept_semantics_20260905/authority_effect/retained_gate}"
CASE_DIR="tests/concept_semantics/authority_effect"
mkdir -p "$WORK_DIR"
cd "$ROOT_DIR"

fail() {
    printf '[concept-authority-effect] %s\n' "$*" >&2
    exit 1
}

[[ -x "$PGY" ]] || fail "native compiler is unavailable"
[[ -x "$SELF_DRIVER" ]] || fail "installed capability owner is unavailable"

for case_name in clock_both_contracts clock_effect_only clock_capability_only \
    authority_named_slot authority_removed_same_capability effect_layer_declared; do
    "$PGY" --native-pipeline --mir-json "$CASE_DIR/$case_name.pgy" \
        >"$WORK_DIR/$case_name.native.out" 2>"$WORK_DIR/$case_name.native.err" \
        || fail "$case_name native positive failed"
    grep -Fq '"pgy.mir.v1"' "$WORK_DIR/$case_name.native.out" \
        || fail "$case_name did not produce MIR"
done

while IFS='|' read -r case_name diagnostic; do
    if "$PGY" --native-pipeline --mir-json "$CASE_DIR/$case_name.pgy" \
        >"$WORK_DIR/$case_name.native.out" 2>"$WORK_DIR/$case_name.native.err"; then
        fail "$case_name native semantic contract was accepted"
    fi
    grep -Fq "$diagnostic" "$WORK_DIR/$case_name.native.err" \
        || fail "$case_name lost its semantic diagnostic"
    if grep -Fq '"pgy.mir.v1"' "$WORK_DIR/$case_name.native.out"; then
        fail "$case_name published MIR after semantic rejection"
    fi
done <<'CASES'
clock_wrong_capability|missing declared capabilities: clock
clock_wrong_effect|missing declared effects: nondeterministic
authority_wrong_same_type_slot|participant 'observer' is not declared in zone authority set
effect_layer_as_class|references unknown effect type 'Marked'
CASES

for case_name in clock_both_contracts clock_effect_only clock_capability_only; do
    "$SELF_DRIVER" --emit-capability-manifest-verified "$CASE_DIR/$case_name.pgy" \
        >"$WORK_DIR/$case_name.manifest.out" 2>"$WORK_DIR/$case_name.manifest.err" \
        || fail "$case_name installed manifest failed"
    grep -Fq '"used": ["CLOCK"]' "$WORK_DIR/$case_name.manifest.out" \
        || fail "$case_name lost inferred CLOCK"
done
cmp -s "$WORK_DIR/clock_both_contracts.manifest.out" \
    "$WORK_DIR/clock_effect_only.manifest.out" \
    || fail "removing the explicit caps assertion changed inferred use"
cmp -s "$WORK_DIR/clock_both_contracts.manifest.out" \
    "$WORK_DIR/clock_capability_only.manifest.out" \
    || fail "removing the explicit effects assertion changed inferred use"

if "$SELF_DRIVER" --emit-capability-manifest-verified \
    "$CASE_DIR/clock_wrong_capability.pgy" \
    >"$WORK_DIR/clock_wrong_capability.manifest.out" \
    2>"$WORK_DIR/clock_wrong_capability.manifest.err"; then
    fail "installed capability owner accepted the missing CLOCK grant"
fi
grep -Fq 'missing declared capabilities: clock' \
    "$WORK_DIR/clock_wrong_capability.manifest.out" \
    || fail "installed capability owner lost its diagnostic"

printf '[concept-authority-effect] native 6 accepted / 4 rejected; installed manifest 3 accepted / 1 rejected; source substitutions preserve inferred CLOCK: PASS\n'
