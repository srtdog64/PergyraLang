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
"$PYTHON" - <<'PY'
import pathlib, re
root = pathlib.Path("src/self_hosted/compiler")
rows = []
for path in root.glob("*expression_kind*owner.pgy"):
    rows += [(name, int(identity)) for name, identity in re.findall(
        r"func (DirectMirScalarProgramExpr\w+)\(\) -> Int \{ return (\d+); \}",
        path.read_text(encoding="utf-8"))]
assert [(n, i) for n, i in rows if i == 146] == [("DirectMirScalarProgramExprArrayDropStorage", 146)], rows
assert [(n, i) for n, i in rows if i == 139] == [("DirectMirScalarProgramExprArraySlice", 139)], rows
assert "return DirectMirScalarProgramExprArrayDropStorage();" in (
    root / "direct_mir_scalar_program_task_expression_kind_owner.pgy").read_text(encoding="utf-8")
native = pathlib.Path("src/semantic/builtin_name_reservation.def").read_text(encoding="utf-8")
selfhost = pathlib.Path("src/self_hosted/semantic/builtin_shadow_owner.pgy").read_text(encoding="utf-8")
release = pathlib.Path("src/semantic/array_storage_release_owner.c").read_text(encoding="utf-8")
assert "function_param_flow_preserves_array_storage(ctx, callee_decl, ordinal)" in release
assert "static bool\narray_storage_plain_element" not in release
# Pass 2 records user-callee inout handoffs; they are decided after every body.
assert "semantic_array_storage_record_pending_call(ctx, handoff, call, callee_decl, ordinal)" in release
assert "semantic_array_storage_record_pending_drop(ctx, receiver, binding)" in release
deferral = pathlib.Path("src/semantic/array_storage_deferred_preservation_owner.c").read_text(encoding="utf-8")
assert "ctx->function_param_flow_summaries = pass2_store;" in deferral
program = pathlib.Path("src/semantic/type_checker_program.c").read_text(encoding="utf-8")
assert program.index("semantic_array_storage_deferral_begin(ctx)") < program.index("type_check_statement(stmt, ctx);")
assert program.index("semantic_array_storage_deferral_finalize(ctx)") < program.index("semantic_collection_owned_element_requirements_finalize(ctx)")
summary = pathlib.Path("src/semantic/function_param_flow_summary.c").read_text(encoding="utf-8")
assert "function_param_array_storage_unproved_in_program_points(" in summary
assert "array_storage_unproved" in summary
storage = pathlib.Path("src/self_hosted/semantic/array_storage_release_parameter_requirement_owner.pgy").read_text(encoding="utf-8")
assert "if mode == 1 { return true; }" not in storage
assert "!MapHas(preserving_formals, ToString(formal))" in storage
mir_release = (root / "direct_mir_array_storage_release_lifetime_owner.pgy").read_text(encoding="utf-8")
assert "DirectMirArrayStoragePreservingParameters(plan)" in mir_release
assert '!MapHas(preserving_parameters, ToString(plan.routines.parameter_starts[target] + argument))' in mir_release
mir_proof = (root / "direct_mir_array_storage_call_preservation_owner.pgy").read_text(encoding="utf-8")
assert "DirectMirScalarProgramArgumentViewFromNode(" in mir_proof
assert "DirectMirScalarProgramDirectCallArgumentRows(" not in mir_proof
assert native.count('PGY_BUILTIN_NAME_RESERVED(TYPED_PROTOCOL, "ArrayDrop")') == 1
assert selfhost.count('"TYPED_PROTOCOL^ArrayDrop"') == 1
assert '"ArrayDrop^Void^Unknown"' in pathlib.Path(
    "src/self_hosted/semantic/builtin_signature_owner.pgy").read_text(encoding="utf-8")
PY
echo '[public-array-drop-owner] PASS (structural inventory; not execution proof)'
