#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_projection_plan.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

require_text() {
    local path="$1"
    local text="$2"
    if ! grep -Fq "$text" "$ROOT_DIR/$path"; then
        echo "[verified-projection-plan] missing '$text' in $path" >&2
        exit 1
    fi
}

for retired in \
    src/common/intent_observability_names.c \
    src/common/intent_observability_names.h \
    src/codegen/intent_observability_usage.c \
    src/codegen/intent_observability_usage.h; do
    if [[ -e "$ROOT_DIR/$retired" ]]; then
        echo "[verified-projection-plan] retired alias still exists: $retired" >&2
        exit 1
    fi
done

require_text src/common/intent_observability_abi.h \
    "typedef struct PgyIntentObservabilityAbiRow"
require_text src/common/intent_observability_abi.h "runtime_call_abi_id"
require_text src/common/intent_observability_abi.h "Append-only identity"
require_text src/common/intent_observability_abi.h "runtime_name"
require_text src/common/intent_observability_abi.h "parameter_shape"
require_text src/common/intent_observability_abi.h \
    "pgy_intent_observability_argument_count"
for shape in \
    PGY_INTENT_OBSERVABILITY_PARAMS_NONE \
    PGY_INTENT_OBSERVABILITY_PARAMS_INT \
    PGY_INTENT_OBSERVABILITY_PARAMS_INT_INT; do
    require_text src/common/intent_observability_abi.def "$shape"
done
require_text src/common/intent_observability_abi.h \
    "PgyIntentObservabilityArgumentKind"
require_text src/common/intent_observability_abi.h "return_kind"
require_text src/compiler/verified_projection_plan.h \
    "typedef struct PgyVerifiedProjectionPlanRow"
require_text src/compiler/verified_projection_plan.h \
    "target_capability_fingerprint"
require_text src/compiler/verified_projection_plan.h \
    "machine_layer_manifest_fingerprint"
require_text src/compiler/verified_projection_plan.h \
    "machine_layer_physical_manifest_fingerprint"
require_text src/compiler/verified_projection_plan.h \
    "machine_layer_physical_grant_base"
require_text src/compiler/verified_projection_plan.h \
    "machine_layer_physical_grant_size"
require_text src/compiler/verified_projection_plan.h \
    "machine_layer_physical_grant_mode"
for term in \
    PGY_PROJECTION_TARGET_C \
    PGY_PROJECTION_TARGET_LLVM \
    PGY_PROJECTION_ERASE \
    PGY_PROJECTION_MATERIALIZE \
    PGY_PROJECTION_RUNTIME_OBS0 \
    PGY_PROJECTION_RUNTIME_OBS1 \
    verified; do
    require_text src/compiler/verified_projection_plan.h "$term"
done
require_text src/compiler/verified_projection_plan.c \
    "verified projection plan: MIR program is missing inventory surface usage facts"
require_text src/compiler/verified_projection_plan.c \
    "mir_program_recorded_inventory_uses_intent_observability_surface(mir)"
require_text src/compiler/verified_projection_plan.c \
    "pgy_verified_projection_plan_intent_observability_with_air"
require_text src/compiler/air_evidence_certificate.h \
    "PGY_AIR_EVIDENCE_CERTIFICATE_SCHEMA"
require_text src/compiler/air_evidence_certificate.c \
    "owner facts changed after verification"
require_text src/compiler/air_verify.c \
    "pgy_air_evidence_certificate_issue"
require_text src/compiler/verified_projection_plan.c \
    "target capability fingerprint is missing"
require_text src/compiler/verified_projection_plan.c \
    "machine-layer manifest fingerprint is missing"
require_text src/compiler/verified_projection_plan.c \
    "pgy_machine_layer_manifest_fingerprint"
require_text src/compiler/verified_projection_plan.c \
    "pgy_machine_layer_physical_manifest_fingerprint"
require_text tests/verified_projection_plan_probe.c \
    "pergyra.machine-declaration.probe-v1"
require_text tests/verified_projection_plan_probe.c \
    "machine_layer_physical_grant_base != grant->base"
require_text tests/verified_projection_plan_probe.c \
    "pgy_machine_layer_runtime_bind_mapping_export"
require_text src/compiler/verified_projection_plan.c \
    "pgy_target_capability_ready_for_projection"
require_text src/codegen/transpiler_entry.c \
    "target capability fingerprint is missing"
require_text src/codegen/llvm_api.c \
    "target capability fingerprint is missing"
require_text src/codegen/transpiler_entry.c \
    "machine-layer manifest fingerprint is missing"
require_text src/codegen/llvm_api.c \
    "machine-layer manifest fingerprint is missing"
require_text src/codegen/transpiler_entry.c \
    "physical machine declaration fingerprint is missing"
require_text src/codegen/llvm_api.c \
    "physical machine declaration fingerprint is missing"
if grep -Fq "pgy_target_capability_ready_for_projection" \
        "$ROOT_DIR/src/codegen/transpiler_entry.c" \
        "$ROOT_DIR/src/codegen/llvm_api.c"; then
    echo "[verified-projection-plan] backend directly consumed target capability SoT" >&2
    exit 1
fi
if grep -Fq "pgy_target_capability_fingerprint" \
        "$ROOT_DIR/src/codegen/transpiler_entry.c" \
        "$ROOT_DIR/src/codegen/llvm_api.c"; then
    echo "[verified-projection-plan] backend recomputed target capability fingerprint" >&2
    exit 1
fi
if grep -Fq "pgy_target_capability_envelope" \
        "$ROOT_DIR/src/codegen/transpiler_entry.c" \
        "$ROOT_DIR/src/codegen/llvm_api.c"; then
    echo "[verified-projection-plan] backend directly read target capability envelope" >&2
    exit 1
fi

if grep -Eq 'ast_|hir_|direct_calls|expr0|expr1|source_ast|mir_routine_inventory' \
        "$ROOT_DIR/src/compiler/verified_projection_plan.c"; then
    echo "[verified-projection-plan] projection owner reintroduced source inference" >&2
    exit 1
fi

for consumer in \
    src/semantic/type_checker_builtins_intent_observability.c \
    src/codegen/transpiler_intent_observability_builtin_emit.c \
    src/codegen/llvm_expr_intent_observability_calls.c \
    src/codegen/llvm_runtime_core_builtin_decl.c; do
    require_text "$consumer" "intent_observability_abi.h"
done
require_text src/semantic/type_checker_builtins_intent_observability.c \
    "pgy_intent_observability_argument_kind_at(row, i)"
require_text src/semantic/type_checker_builtins_intent_observability.c \
    "pgy_intent_observability_argument_count(row)"
require_text src/codegen/llvm_runtime_core_builtin_decl.c \
    "pgy_intent_observability_argument_kind_at(row, j)"
require_text src/semantic/type_checker_builtins_resolve.c \
    "pgy_intent_observability_name_is_builtin(name)"
require_text src/codegen/transpiler_entry.c \
    "PGY_PROJECTION_TARGET_C"
require_text src/codegen/llvm_api.c \
    "PGY_PROJECTION_TARGET_LLVM"
if grep -Fq "pgy_verified_projection_plan_intent_observability(" \
        "$ROOT_DIR/src/codegen/transpiler_entry.c" \
        "$ROOT_DIR/src/codegen/llvm_api.c"; then
    echo "[verified-projection-plan] production backend retained MIR-only planner path" >&2
    exit 1
fi
require_text scripts/ci_linux_steps.sh \
    'BUILD_DIR="$CI_LINUX_BUILD_DIR" BIN_DIR="$CI_LINUX_BIN_DIR" verified-projection-plan-test-smoke'
require_text scripts/ci_macos_steps.sh \
    'BUILD_DIR="$CI_MACOS_BUILD_DIR" BIN_DIR="$CI_MACOS_BIN_DIR" verified-projection-plan-test-smoke'
require_text scripts/ci_windows_steps.sh \
    'BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" verified-projection-plan-test-smoke'

if grep -Eq 'pgy_intent_(last|history|active|current|recent)_[a-z_]*_export' \
        "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c" \
        "$ROOT_DIR/src/codegen/llvm_runtime_core_builtin_decl.c"; then
    echo "[verified-projection-plan] backend-local observability ABI spelling returned" >&2
    exit 1
fi
if grep -Eq '"Intent(Active|Current|History|Last|Recent)' \
        "$ROOT_DIR/src/common/pgy_builtin_type_table.c" \
        "$ROOT_DIR/src/semantic/type_checker_builtins_resolve.c" \
        "$ROOT_DIR/src/semantic/type_checker_builtins_intent_observability.c" \
        "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c"; then
    echo "[verified-projection-plan] observability source-name alias returned" >&2
    exit 1
fi
if grep -RIn 'BUILTIN_INTENT_\(LAST\|HISTORY\|ACTIVE\|CURRENT\|RECENT\)' \
        "$ROOT_DIR/src" --include='*.c' --include='*.h'; then
    echo "[verified-projection-plan] per-call observability BuiltinKind aliases returned" >&2
    exit 1
fi

abi_names="$(
    grep -o '"Intent[A-Za-z0-9_]*"' \
        "$ROOT_DIR/src/common/intent_observability_abi.def" | tr -d '"'
)"
if [[ "$(printf '%s\n' "$abi_names" | sed '/^$/d' | wc -l | tr -d ' ')" != "51" ]]; then
    echo "[verified-projection-plan] expected 51 canonical observability ABI rows" >&2
    exit 1
fi
if [[ "$abi_names" != "$(printf '%s\n' "$abi_names" | sort)" ]]; then
    echo "[verified-projection-plan] ABI rows must remain source-name sorted" >&2
    exit 1
fi

cp "$ROOT_DIR/src/compiler/verified_projection_plan.c" "$WORK_DIR/mutated.c"
printf '\nvoid mutation(void) { ast_contains_identifier_call(0, 0, 0); }\n' \
    >> "$WORK_DIR/mutated.c"
if ! grep -Eq 'ast_|hir_|direct_calls|expr0|expr1|source_ast|mir_routine_inventory' \
        "$WORK_DIR/mutated.c"; then
    echo "[verified-projection-plan] negative source-inference mutation escaped" >&2
    exit 1
fi

PROBE="${PGY_VERIFIED_PROJECTION_PLAN_PROBE:-}"
if [[ -z "$PROBE" || ! -x "$PROBE" ]]; then
    echo "[verified-projection-plan] missing compiled probe: $PROBE" >&2
    exit 1
fi
"$PROBE"

echo "[verified-projection-plan] canonical ABI and fail-closed projection owner are closed"
