#include "transpiler_mir_match_payload_emit.h"

#include <string.h>

#include "../../semantic/diag_codes.h"
#include "../../compiler/mir/mir_branch_source_facts.h"
#include "transpiler_context.h"
#include "transpiler_format.h"
#include "transpiler_mir_match_pattern_emit.h"
#include "transpiler_symbols.h"
#include "transpiler_type_require.h"

static bool
transpiler_mir_match_payload_type_name(TranspilerCtx *ctx,
                                       uint32_t match_case_syntax_id,
                                       char *buf,
                                       size_t buf_size)
{
    const MIRRoutine *routine;
    const MIRMatchBindingTypeFact *fact;
    size_t length;

    if (buf == NULL || buf_size == 0)
        return false;
    buf[0] = '\0';
    if (ctx == NULL || ctx->active_mir_routine == NULL)
        return false;
    routine = ctx->active_mir_routine;
    fact = mir_routine_match_binding_type_fact(
        routine, match_case_syntax_id, 0);
    if (fact == NULL || routine->source_syntax_id == 0
        || fact->function_syntax_id != routine->source_syntax_id
        || fact->binding_syntax_id == 0 || fact->binding_count != 1
        || fact->binding_type_name == NULL
        || fact->binding_type_name[0] == '\0'
        || strcmp(fact->binding_type_name, "Unknown") == 0)
        return false;
    length = strlen(fact->binding_type_name);
    if (length >= buf_size)
        return false;
    memcpy(buf, fact->binding_type_name, length + 1);
    return true;
}

void
transpiler_mir_emit_match_payload_binding(CodeBuf *buf,
                                          TranspilerCtx *ctx,
                                          uint32_t match_case_syntax_id,
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
    if (!transpiler_mir_match_payload_type_name(ctx, match_case_syntax_id,
                                                payload_type,
                                                sizeof(payload_type))) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C MIR match lowering requires an admitted binding type fact for %s(%s)",
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
                                             uint32_t match_case_syntax_id,
                                             const char *kind,
                                             const char *binding,
                                             const char *emitted_name)
{
    const char *payload_c_type;
    char payload_type[128];

    if (buf == NULL || ctx == NULL
        || kind == NULL || binding == NULL || emitted_name == NULL) {
        return true;
    }
    if (!transpiler_mir_match_payload_type_name(ctx, match_case_syntax_id,
                                                payload_type,
                                                sizeof(payload_type))) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C MIR match guard requires an admitted binding type fact for %s(%s)",
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
