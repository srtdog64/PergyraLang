#!/usr/bin/env bash
# Builtins retain an occupied unqualified callable name while enum variants
# keep their declaration-owned qualified identity. The installed source-to-MIR
# path must agree with the explicit native oracle without retrying it.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-semantic-enum-variant-builtin-collision"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/semantic_enum_variant_builtin_collision"
WORK_DIR="$ROOT_DIR/$WORK_REL"
POSITIVE_REL="tests/cases/backend_compare/tagged_union/main.pgy"
ADJACENT_REL="src/self_hosted/mir_lower/fixture/enum_multi_payload.pgy"
NEGATIVE_REL="tests/self_hosted/parity/fixture/enum_variant_builtin_collision_rejected.pgy"
POLICY_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_function_table_fact_owner.pgy"
CONSUMER_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_environment_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$SELF_DRIVER" || exit 1

PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
installed_name="pgy-self-driver"
[[ "$PGY" == *.exe ]] && installed_name="pgy-self-driver.exe"
[[ "$SELF_DRIVER" == "$(dirname "$PGY")/$installed_name" ]] ||
    fail "self-host driver is not installed beside the public launcher"

grep -Fq 'func SemanticAstUnqualifiedEnumVariantMayOwnCallableRow(' \
    "$POLICY_OWNER" || fail "unqualified enum collision policy owner is missing"
grep -Fq 'SemanticAstUnqualifiedEnumVariantMayOwnCallableRow(existing)' \
    "$CONSUMER_OWNER" || fail "enum callable projection bypasses the policy owner"
! grep -Fq '"None"' "$POLICY_OWNER" "$CONSUMER_OWNER" ||
    fail "builtin spelling leaked into the enum collision policy"
[[ "$(grep -Fc 'returns[existing] != enum_name' "$CONSUMER_OWNER")" -eq 1 ]] ||
    fail "unqualified enum collision regained the qualified-row mismatch policy"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

produce_triplet() {
    local label="$1" source="$2"
    (cd "$ROOT_DIR" && unset PGY_IO_ROOT && PGY_IO_ALLOW_ABSOLUTE=1 \
        "$SELF_DRIVER" --emit-mir-json-verified "$ROOT_DIR/$source") \
        >"$WORK_DIR/$label.direct.json" 2>"$WORK_DIR/$label.direct.err" ||
        fail "$label direct installed MIR production failed"
    (cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN &&
        PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" --mir-json "$source") \
        >"$WORK_DIR/$label.public.json" 2>"$WORK_DIR/$label.public.err" ||
        fail "$label public installed MIR production failed"
    cmp -s "$WORK_DIR/$label.direct.json" "$WORK_DIR/$label.public.json" ||
        fail "$label public MIR differs from its installed owner"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$label.public.err" ||
        fail "$label public MIR retried the native pipeline"
    grep -Fq '"schema":"pgy.mir.v1"' "$WORK_DIR/$label.public.json" ||
        fail "$label installed owner emitted no MIR artifact"

    (cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$source") \
        >"$WORK_DIR/$label.native.json" 2>"$WORK_DIR/$label.native.err" ||
        fail "$label explicit native oracle failed"
    (cd "$ROOT_DIR" && "$SELF_DRIVER" --canonicalize-oracle-mir-json \
        "$WORK_REL/$label.native.json") \
        >"$WORK_DIR/$label.native.canonical.json"
    (cd "$ROOT_DIR" && "$SELF_DRIVER" --canonicalize-mir-json \
        "$WORK_REL/$label.public.json") \
        >"$WORK_DIR/$label.self.canonical.json"
    cmp -s "$WORK_DIR/$label.native.canonical.json" \
        "$WORK_DIR/$label.self.canonical.json" ||
        fail "$label installed MIR diverged from the native oracle"
}

produce_triplet tagged-union "$POSITIVE_REL"
produce_triplet adjacent-enum "$ADJACENT_REL"

set +e
(cd "$ROOT_DIR" && unset PGY_IO_ROOT && PGY_IO_ALLOW_ABSOLUTE=1 \
    "$SELF_DRIVER" --emit-mir-json-verified "$ROOT_DIR/$NEGATIVE_REL") \
    >"$WORK_DIR/negative.direct.out" 2>"$WORK_DIR/negative.direct.err"
direct_rc=$?
(cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN &&
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" --mir-json "$NEGATIVE_REL") \
    >"$WORK_DIR/negative.public.out" 2>"$WORK_DIR/negative.public.err"
public_rc=$?
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$NEGATIVE_REL") \
    >"$WORK_DIR/negative.native.out" 2>"$WORK_DIR/negative.native.err"
native_rc=$?
set -e

[[ "$direct_rc" -ne 0 && "$public_rc" -eq "$direct_rc" &&
    "$native_rc" -ne 0 ]] || fail "occupied builtin precedence was not rejected"
tr -d '\r' <"$WORK_DIR/negative.direct.out" >"$WORK_DIR/negative.direct.norm"
tr -d '\r' <"$WORK_DIR/negative.public.out" >"$WORK_DIR/negative.public.norm"
cmp -s "$WORK_DIR/negative.direct.norm" "$WORK_DIR/negative.public.norm" ||
    fail "public rejection differs from the installed semantic owner"
grep -Fq 'let_type_mismatch' "$WORK_DIR/negative.direct.norm" ||
    fail "installed rejection lost the typed mismatch identity"
grep -Fq "cannot assign 'Option<<unknown>>' to 'Shape'" \
    "$WORK_DIR/negative.native.err" "$WORK_DIR/negative.native.out" ||
    fail "native oracle no longer preserves builtin precedence"
! grep -Fq '"schema":"pgy.mir.v1"' \
    "$WORK_DIR/negative.direct.out" "$WORK_DIR/negative.public.out" ||
    fail "rejected collision published MIR"
! grep -Fq '[pipeline timing]' "$WORK_DIR/negative.public.err" ||
    fail "public rejection retried the native pipeline"

echo "[$LABEL] installed/public/native MIR parity, builtin precedence negative, and adjacent enum: PASS"
