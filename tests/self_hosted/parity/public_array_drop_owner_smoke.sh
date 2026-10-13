#!/usr/bin/env bash
# Structural inventory only; public_array_drop.sh owns behavioral evidence.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT_DIR"
PYTHON="${PYTHON_BIN:-python3}"
"$PYTHON" scripts/source_size_count.py --caps <<'CAPS'
150	src/semantic/array_storage_release_owner.c
180	src/semantic/array_storage_deferred_preservation_owner.c
60	src/semantic/array_storage_element_lifetime_owner.c
170	src/semantic/function_param_array_storage_transfer_owner.c
210	src/self_hosted/semantic/array_storage_call_preservation_owner.pgy
80	src/self_hosted/semantic/ast_expression_graph_surface_order_owner.pgy
240	src/self_hosted/semantic/array_storage_release_verdict_owner.pgy
110	src/self_hosted/semantic/array_storage_release_parameter_requirement_owner.pgy
60	src/self_hosted/semantic/array_storage_element_lifetime_owner.pgy
360	src/self_hosted/compiler/direct_mir_array_storage_release_lifetime_owner.pgy
180	src/self_hosted/compiler/direct_mir_array_storage_call_preservation_owner.pgy
60	src/self_hosted/compiler/direct_mir_array_storage_element_lifetime_owner.pgy
130	src/self_hosted/compiler/direct_mir_scalar_program_direct_call_readiness_owner.pgy
30	src/self_hosted/compiler/direct_mir_scalar_program_owned_array_value_parameter_policy_owner.pgy
CAPS
echo '[public-array-drop-owner] PASS (structural inventory; not execution proof)'
