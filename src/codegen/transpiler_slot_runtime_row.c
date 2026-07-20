#include "transpiler_slot_runtime_row.h"

#include "../compiler/mir_abi_layout.h"
#include "../compiler/mir_machine_layer.h"
#include "../semantic/diag_codes.h"

#include "transpiler_context.h"

#include <string.h>

const MIRResourceRuntimeRow *
transpiler_slot_runtime_row_for_operation(TranspilerCtx *ctx,
                                          bool secure,
                                          const char *inner_type,
                                          const char *operation)
{
    const MIRInstruction *inst = ctx != NULL
        ? ctx->active_mir_instruction
        : NULL;
    const MIRResourceRuntimeRow *row = NULL;
    const char *expected_shape =
        transpiler_slot_runtime_expected_call_shape(secure, operation);

    if (inst != NULL && inst->resource_runtime_fact_present) {
        row = &inst->resource_runtime_fact;
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
    } else if (inst != NULL && inst->kind == MIR_INST_RESOURCE_OP
               && operation != NULL
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
                   || strcmp(operation, "SubmitRead") == 0)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C MIR resource operation is missing its lowered runtime-call ABI row");
        return NULL;
    } else {
        row = mir_abi_resource_runtime_row_by_kind(
            secure ? MIR_RESOURCE_ABI_SECURE_SLOT : MIR_RESOURCE_ABI_SLOT,
            inner_type,
            operation);
    }

    if (row == NULL || row->runtime_fn == NULL || row->call_shape == NULL
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
    if (inst != NULL
        && rir_machine_contact_kind_is_present(inst->machine_contact_kind)
        && !mir_machine_layer_fact_matches_runtime_operation(
            inst, row->resource_op_name)) {
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
