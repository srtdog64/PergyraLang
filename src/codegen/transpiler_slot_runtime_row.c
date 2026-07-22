#include "transpiler_slot_runtime_row.h"

#include "../compiler/mir_decl_field_claim_abi.h"
#include "../compiler/mir_abi_layout.h"
#include "../compiler/mir_machine_layer.h"
#include "../semantic/diag_codes.h"

#include "transpiler_context.h"

#include <string.h>

static bool
transpiler_slot_runtime_operation_requires_lowered_row(const char *operation)
{
    return operation != NULL
        && (strcmp(operation, "Claim") == 0
            || strcmp(operation, "Read") == 0
            || strcmp(operation, "Write") == 0
            || strcmp(operation, "Release") == 0
            || strcmp(operation, "PinRead") == 0
            || strcmp(operation, "PinWrite") == 0
            || strcmp(operation, "PinReadInit") == 0
            || strcmp(operation, "PinWriteInit") == 0
            || strcmp(operation, "Unpin") == 0
            || strcmp(operation, "UnpinCleanup") == 0
            || strcmp(operation, "SubmitRead") == 0);
}

const MIRResourceRuntimeRow *
transpiler_slot_runtime_row_for_source_operation(TranspilerCtx *ctx,
                                                 const ASTNode *source_call,
                                                 bool secure,
                                                 const char *inner_type,
                                                 const char *operation)
{
    const MIRInstruction *inst = ctx != NULL
        ? ctx->active_mir_instruction
        : NULL;
    const MIRInstruction *source_inst = NULL;
    const MIRResourceRuntimeRow *row = NULL;
    bool row_is_mir_fact = false;
    const char *expected_shape =
        transpiler_slot_runtime_expected_call_shape(secure, operation);

    if (ctx != NULL && ctx->active_mir_routine != NULL) {
        if (source_call != NULL) {
            source_inst = mir_abi_resource_runtime_instruction_for_source(
                ctx->active_mir_routine,
                ast_node_stable_id(source_call));
            if (source_inst != NULL) {
                const MIRResourceRuntimeRow *source_row =
                    mir_abi_resource_runtime_row_for_instruction(
                        source_inst, operation);
                if (source_row != NULL
                    && source_row->resource_op_name != NULL
                    && operation != NULL
                    && strcmp(source_row->resource_op_name, operation) == 0) {
                    row = source_row;
                    row_is_mir_fact = true;
                } else {
                    source_inst = NULL;
                }
            }
        }
        if (row == NULL && transpiler_slot_runtime_operation_requires_lowered_row(
                operation)) {
            source_inst = mir_abi_resource_runtime_instruction_for_abi(
                ctx->active_mir_routine,
                secure ? MIR_RESOURCE_ABI_SECURE_SLOT : MIR_RESOURCE_ABI_SLOT,
                inner_type,
                operation);
            if (source_inst != NULL) {
                row = mir_abi_resource_runtime_row_for_instruction(
                    source_inst, operation);
                row_is_mir_fact = true;
            }
            if (row == NULL) {
                row = mir_abi_resource_runtime_row_for_mir_abi(
                    ctx->active_mir_routine,
                    secure ? MIR_RESOURCE_ABI_SECURE_SLOT
                           : MIR_RESOURCE_ABI_SLOT,
                    inner_type,
                    operation);
                if (row != NULL)
                    row_is_mir_fact = true;
                if (row != NULL)
                    source_inst = mir_abi_resource_runtime_owner_for_mir_abi(
                        ctx->active_mir_routine,
                        secure ? MIR_RESOURCE_ABI_SECURE_SLOT
                               : MIR_RESOURCE_ABI_SLOT,
                        inner_type);
            }
        }
    }
    if (row != NULL) {
        /* Exact source identity wins over the enclosing instruction row for
         * nested resource expressions such as Write(a, Read(b)). */
    } else if (inst != NULL && inst->resource_runtime_fact_present) {
        row = mir_abi_resource_runtime_row_for_instruction(inst, operation);
        row_is_mir_fact = true;
        if (row->resource_op_name == NULL || operation == NULL
            || strcmp(row->resource_op_name, operation) != 0) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C MIR resource operation has a mismatched runtime-call ABI row");
            return NULL;
        }
        if (row->runtime_call_abi_id == 0
            || row->runtime_call_abi_id
                != mir_abi_resource_runtime_row_id(row)
            || !mir_abi_resource_runtime_row_matches_owner(row)) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C MIR resource operation has an invalid runtime-call ABI identity");
            return NULL;
        }
    } else if (inst != NULL && inst->kind == MIR_INST_RESOURCE_OP
               && transpiler_slot_runtime_operation_requires_lowered_row(
                   operation)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C MIR resource operation is missing its lowered runtime-call ABI row");
        return NULL;
    } else if (transpiler_active_has_mir(ctx)
               && transpiler_slot_runtime_operation_requires_lowered_row(
                   operation)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C MIR source operation has no active instruction-owned runtime-call ABI row");
        return NULL;
    } else if (transpiler_active_has_mir(ctx)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "MIR-only C path missing active routine for runtime-call ABI row");
        return NULL;
    } else {
        row = mir_abi_resource_runtime_row_by_kind(
            secure ? MIR_RESOURCE_ABI_SECURE_SLOT : MIR_RESOURCE_ABI_SLOT,
            inner_type,
            operation);
    }

    if (row_is_mir_fact && row != NULL
        && (row->runtime_call_abi_id == 0
            || row->runtime_call_abi_id
                != mir_abi_resource_runtime_row_id(row)
            || !mir_abi_resource_runtime_row_matches_owner(row))) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C MIR resource operation has an invalid runtime-call ABI identity");
        return NULL;
    }
    if (row == NULL || row->resource_op_name == NULL || operation == NULL
        || strcmp(row->resource_op_name, operation) != 0
        || row->runtime_fn == NULL || row->call_shape == NULL
        || (expected_shape != NULL
            && strcmp(row->call_shape, expected_shape) != 0)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C slot operation %s requires MIR ABI runtime function row",
            operation != NULL ? operation : "<unknown>");
        return NULL;
    }
    if (transpiler_active_has_mir(ctx)) {
        const MIRInstruction *layout_inst = source_inst != NULL
            ? source_inst
            : inst;
        if ((layout_inst == NULL || layout_inst->type_layout == NULL
             || layout_inst->abi_layout_id == 0
             || layout_inst->abi_layout_id
                 != mir_abi_layout_id(layout_inst->type_layout))
            && !mir_abi_resource_runtime_row_is_constructed_nominal(row)) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C MIR source operation has a missing or mismatched ABI layout identity");
            return NULL;
        }
    }
    const MIRInstruction *machine_inst = source_inst != NULL
        ? source_inst
        : inst;
    if (machine_inst != NULL
        && rir_machine_contact_kind_is_present(machine_inst->machine_contact_kind)
        && !mir_machine_layer_fact_matches_runtime_operation(
            machine_inst, row->resource_op_name)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C slot runtime row disagrees with machine-layer runtime operation");
        return NULL;
    }
    return row;
}

const MIRResourceRuntimeRow *
transpiler_slot_runtime_row_for_operation(TranspilerCtx *ctx,
                                          bool secure,
                                          const char *inner_type,
                                          const char *operation)
{
    return transpiler_slot_runtime_row_for_source_operation(
        ctx, NULL, secure, inner_type, operation);
}

const char *
transpiler_slot_runtime_expected_call_shape(bool secure, const char *operation)
{
    if (operation == NULL)
        return NULL;
    if (strcmp(operation, "Claim") == 0)
        return secure ? "token_ptr_to_container" : "returns_container";
    if (strcmp(operation, "Read") == 0)
        return secure ? "container_ptr_token_ptr_to_value"
                      : "container_ptr_to_value";
    if (strcmp(operation, "Write") == 0)
        return secure ? "container_ptr_value_token_ptr_to_void"
                      : "container_ptr_value_to_void";
    if (strcmp(operation, "Release") == 0)
        return secure ? "container_ptr_token_ptr_to_void"
                      : "container_ptr_to_void";
    if (strcmp(operation, "PinRead") == 0 ||
        strcmp(operation, "PinWrite") == 0) {
        return secure ? "container_ptr_token_ptr_to_pinned_view"
                      : "container_ptr_to_pinned_view";
    }
    if (strcmp(operation, "Unpin") == 0 ||
        strcmp(operation, "UnpinCleanup") == 0) {
        return "pinned_view_ptr_to_void";
    }
    return NULL;
}

const char *
transpiler_slot_runtime_fn(TranspilerCtx *ctx,
                           bool secure,
                           const char *inner_type,
                           const char *operation)
{
    const MIRResourceRuntimeRow *row =
        transpiler_slot_runtime_row_for_operation(
            ctx, secure, inner_type, operation);
    return row != NULL ? row->runtime_fn : NULL;
}

const char *
transpiler_slot_runtime_fn_for_decl_claim(TranspilerCtx *ctx,
                                          const MIRDeclFieldClaim *claim)
{
    if (!mir_decl_field_claim_abi_validate(claim)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C MIR class field claim is missing its declaration-owned runtime-call ABI row");
        return NULL;
    }
    return claim->runtime_call_abi.runtime_fn;
}

void
transpiler_emit_nominal_container_runtime_rows(CodeBuf *dst,
                                               const char *type_name,
                                               bool include_intro_comment)
{
    if (dst == NULL || type_name == NULL)
        return;

    if (include_intro_comment) {
        codebuf_write(dst,
            "\n/* Auto-generated container types for %s */\n",
            type_name);
    } else {
        codebuf_write(dst, "\n");
    }

    codebuf_write(dst,
        "#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n"
        "#pragma GCC diagnostic pop\n",
        type_name, type_name,
        type_name, type_name,
        type_name, type_name);
}
