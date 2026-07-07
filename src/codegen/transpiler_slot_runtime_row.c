#include "transpiler_slot_runtime_row.h"

#include "../compiler/mir_abi_layout.h"
#include "../semantic/diag_codes.h"

#include "transpiler_context.h"

const char *
transpiler_slot_runtime_fn(TranspilerCtx *ctx,
                           bool secure,
                           const char *inner_type,
                           const char *operation)
{
    const char *runtime_fn = mir_abi_resource_runtime_fn_by_kind(
        secure ? MIR_RESOURCE_ABI_SECURE_SLOT : MIR_RESOURCE_ABI_SLOT,
        inner_type, operation);
    if (runtime_fn != NULL)
        return runtime_fn;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "C expression slot %s requires MIR ABI runtime function row",
        operation != NULL ? operation : "<unknown>");
    return NULL;
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
