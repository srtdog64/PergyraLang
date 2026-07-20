#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

fail() {
    echo "[mir-only-signature] $*" >&2
    exit 1
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$rel" || fail "$rel missing term: $term"
}

forbid_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$rel"; then
        fail "$rel reopened AST signature recovery: $term"
    fi
}

SIG="src/codegen/transpiler_mir_signature.c"
SIG_H="src/codegen/transpiler_mir_signature.h"
CONTRACT="src/codegen/transpiler_mir_emission_contract.c"
BODY="src/codegen/transpiler_mir_func_emit.c"
FORWARD="src/codegen/transpiler_func_forward_emit.c"
POLICY="src/codegen/transpiler_func_forward_policy.c"
DECL="src/codegen/transpiler_type_declarator.c"
ENTRY="src/compiler/compiler.c"
C_BACKEND_ENTRY="src/codegen/transpiler_entry.c"
C_BACKEND_PROGRAM="src/codegen/transpiler.c"
LLVM_ENTRY="src/compiler/compiler_llvm.c"
LLVM_API="src/codegen/llvm_api.c"
LLVM_HEADER="src/codegen/llvm_backend.h"

require_text "$SIG_H" "transpiler_mir_routine_signature_supported_strict"
require_text "$SIG" "transpiler_mir_routine_signature_supported_strict"
require_text "$SIG" "MIR-only C path has incomplete function return callable metadata"
require_text "$SIG" "MIR-only C path missing function parameter type-name metadata"
require_text "$CONTRACT" "transpiler_mir_routine_signature_supported_strict"
require_text "$CONTRACT" "transpiler_active_has_mir(ctx)"
require_text "$BODY" "transpiler_mir_routine_signature_supported_strict"
require_text "$BODY" "transpiler_active_has_mir(ctx)"
require_text "$FORWARD" "transpiler_mir_routine_signature_supported_strict"
require_text "$POLICY" "transpiler_mir_routine_signature_supported_strict"

for rel in "$CONTRACT" "$BODY" "$FORWARD" "$POLICY"; do
    forbid_text "$rel" "transpiler_mir_ast_type_supported"
done

require_text "$BODY" "transpiler_mir_routine_param_callable_sig"
require_text "$BODY" "pergyra_func_signature_declarator_from_callable_sig_in_ctx"
require_text "$FORWARD" "transpiler_mir_routine_param_callable_sig"
require_text "$FORWARD" "pergyra_func_pointer_declarator_from_type_names_in_ctx"
require_text "$DECL" "pergyra_func_signature_declarator_from_callable_sig_in_ctx"

# The backend admission remains AIR-bound; MIR-only is not a second pipeline.
require_text "$ENTRY" "pgy_verified_projection_plan_intent_observability_with_air"
require_text "$ENTRY" "bundle->mir"
require_text "$ENTRY" "air"
require_text "$ENTRY" "pgy_verified_spawn_lane_plan_from_air"
require_text "$C_BACKEND_ENTRY" "pgy_verified_projection_plan_identity_ready"
require_text "$C_BACKEND_ENTRY" "PGY_SPAWN_LANE_PLAN_REVISION"
require_text "$C_BACKEND_PROGRAM" "transpiler_active_decl_header_inventory"
require_text "$C_BACKEND_PROGRAM" "mir_decl_header_name(header)"
require_text "$C_BACKEND_PROGRAM" "MIR nominal declaration header is missing its name"
forbid_text "$C_BACKEND_PROGRAM" "transpiler_decl_name_local(type_decl)"
require_text "$LLVM_ENTRY" "pgy_verified_projection_plan_intent_observability_with_air"
require_text "$LLVM_ENTRY" "pgy_verified_spawn_lane_plan_from_air"
require_text "$LLVM_API" "verified spawn-lane plan required"
require_text "$LLVM_API" "PGY_SPAWN_LANE_PLAN_REVISION"
require_text "$LLVM_API" "pgy_verified_projection_plan_identity_ready"
require_text "$LLVM_HEADER" "PgySpawnLanePlan"

echo "[mir-only-signature] strict MIR signature admission and AIR-bound projection checks ok"
