#include "transpiler_mir_match_payload_emit.h"

#include <string.h>

#include "../semantic/diag_codes.h"
#include "codegen_match_variant_policy.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_mir_match_pattern_emit.h"
#include "transpiler_symbols.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

static bool
transpiler_mir_match_payload_type_name(TranspilerCtx *ctx,
                                       ASTNode *subject_node,
                                       const char *kind,
                                       char *buf,
                                       size_t buf_size)
{
    const char *subject_type;

    if (buf == NULL || buf_size == 0)
        return false;
    buf[0] = '\0';
    if (ctx == NULL || subject_node == NULL || kind == NULL)
        return false;

    subject_type = infer_expression_type_name(ctx, subject_node);
    if (subject_type == NULL || subject_type[0] == '\0')
        return false;

    PgyCodegenMatchVariantKind variant =
        pgy_codegen_match_variant_lookup(kind);

    if (variant == PGY_MATCH_VARIANT_SOME) {
        if (!transpiler_type_name_is_option(subject_type))
            return false;
        return slot_inner_type_name_copy(subject_type, buf, buf_size)
            && buf[0] != '\0';
    }
    if (variant == PGY_MATCH_VARIANT_OK) {
        if (!transpiler_type_name_is_result(subject_type))
            return false;
        copy_constructed_arg_name_at(subject_type, 0, buf, buf_size);
        return buf[0] != '\0' && strcmp(buf, "Unknown") != 0;
    }
    if (variant == PGY_MATCH_VARIANT_ERR) {
        if (!transpiler_type_name_is_result(subject_type))
            return false;
        copy_constructed_arg_name_at(subject_type, 1, buf, buf_size);
        return buf[0] != '\0' && strcmp(buf, "Unknown") != 0;
    }

    return false;
}

void
transpiler_mir_emit_match_payload_binding(CodeBuf *buf,
                                          TranspilerCtx *ctx,
                                          ASTNode *subject_node,
                                          const char *subject,
                                          const char *kind,
                                          const char *binding,
                                          const char *emitted_name)
{
    const char *field;
    const char *payload_c_type;
    char payload_type[128];

    if (buf == NULL || ctx == NULL || subject == NULL
        || kind == NULL || binding == NULL || emitted_name == NULL) {
        return;
    }

    field = transpiler_mir_match_payload_field(kind);
    if (field == NULL)
        return;
    if (!transpiler_mir_match_payload_type_name(ctx, subject_node, kind,
                                                payload_type,
                                                sizeof(payload_type))) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C MIR match lowering cannot derive payload type for %s(%s); explicit Option<T>/Result<T,E> subject type is required",
            kind,
            binding);
        return;
    }

    {
        char payload_c_type_buf[256];
        if (!transpiler_require_type_name_c_type_copy(ctx, payload_type,
                "MIR match payload", payload_c_type_buf,
                sizeof(payload_c_type_buf))) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C MIR match lowering cannot render payload type '%s' for %s(%s)",
                payload_type,
                kind,
                binding);
            return;
        }
        payload_c_type = payload_c_type_buf;

        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s %s = (%s).%s;\n",
                      payload_c_type, emitted_name, subject, field);
        register_typed_var(ctx, binding, payload_type);
        register_typed_var(ctx, emitted_name, payload_type);
    }
}

bool
transpiler_mir_declare_guard_payload_binding(CodeBuf *buf,
                                             TranspilerCtx *ctx,
                                             ASTNode *subject_node,
                                             const char *kind,
                                             const char *binding,
                                             const char *emitted_name)
{
    const char *payload_c_type;
    char payload_type[128];

    if (buf == NULL || ctx == NULL || subject_node == NULL
        || kind == NULL || binding == NULL || emitted_name == NULL) {
        return true;
    }
    if (!transpiler_mir_match_payload_type_name(ctx, subject_node, kind,
                                                payload_type,
                                                sizeof(payload_type))) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C MIR match guard cannot derive payload type for %s(%s); explicit Option<T>/Result<T,E> subject type is required",
            kind,
            binding);
        return false;
    }

    {
        char payload_c_type_buf[256];
        if (!transpiler_require_type_name_c_type_copy(ctx, payload_type,
                "MIR match guard payload", payload_c_type_buf,
                sizeof(payload_c_type_buf))) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C MIR match guard cannot render payload type '%s' for %s(%s)",
                payload_type,
                kind,
                binding);
            return false;
        }
        payload_c_type = payload_c_type_buf;

        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s %s;\n", payload_c_type, emitted_name);
        register_typed_var(ctx, binding, payload_type);
        register_typed_var(ctx, emitted_name, payload_type);
    }
    return true;
}
