#include "transpiler_let_channel_emit.h"

#include <stdlib.h>

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include "parser/ast_api.h"
#include "codegen_channel_runtime_abi.h"
#include "transpiler_context.h"
#include "transpiler_format.h"
#include "transpiler_symbols.h"
#include "codegen_type_mapping.h"

bool
transpiler_try_emit_channel_let(TranspilerCtx *ctx, const char *name,
                                ASTNode *init, char **ann_type_name_io)
{
    char *ann_type_name = ann_type_name_io != NULL ? *ann_type_name_io : NULL;
    if (!transpiler_type_name_is_channel(ann_type_name))
        return false;

    char inner_buf[128];
    const char *inner = NULL;
    char *capacity = pergyra_strdup("16");
    if (slot_inner_type_name_copy(ann_type_name, inner_buf, sizeof(inner_buf)))
        inner = inner_buf;
    if (inner == NULL || inner[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: Channel binding '%s' requires concrete Channel<T> annotation",
            name != NULL ? name : "<binding>");
        free(capacity);
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }
    if (!pgy_channel_runtime_payload_has_abi(inner)) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: Channel binding '%s' has no runtime Channel<%s> ABI; beta runtime supports %s",
            name != NULL ? name : "<binding>",
            inner,
            pgy_channel_runtime_payload_supported_list());
        free(capacity);
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }

    if (init != NULL && init->type == AST_CALL && ast_call_arg_count(init) > 0) {
        free(capacity);
        capacity = emit_expression(ast_call_argument(init, 0), ctx);
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "PgyChannel_%s %s;\n", inner, name);
    write_indent(ctx);
    codebuf_write(ctx->out, "pgy_channel_init_%s(&%s, %s);\n",
        inner, name, capacity);
    register_typed_var(ctx, name, ann_type_name);
    free(capacity);
    free(ann_type_name);
    *ann_type_name_io = NULL;
    return true;
}
