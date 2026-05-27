#include "transpiler_mir_inventory_intent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_type_declarator.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"

void
emit_func_forward_decl_named(ASTNode *node, const char *emitted_name,
                             CodeBuf *buf, TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL ? emitted_name : ast_declaration_name(node);
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    ASTNode *return_type = ast_func_return_type(node);
    ensure_type_specializations_from_ast(ctx, return_type);
    for (size_t i = 0; i < ast_func_param_count(node); i++) {
        FuncParam *p = ast_func_param(node, i);
        const char *pt = NULL;
        char pt_buf[256];
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p->type != NULL)
            ensure_type_specializations_from_ast(ctx, p->type);
        if (p->type != NULL) {
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "forward declaration parameter '%s' of '%s'",
                p->name != NULL ? p->name : "(anonymous)",
                name != NULL ? name : "(anonymous)");
            if (transpiler_require_ast_c_type_copy(ctx, p->type,
                    surface_desc, pt_buf, sizeof(pt_buf))) {
                pt = pt_buf;
            }
        }
        if (pt == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine parameter type for forward declaration '%s' at argument %llu",
                name != NULL ? name : "<function>",
                (unsigned long long) i);
            if (params_sig != NULL)
                codebuf_destroy(params_sig);
            free(header_decl);
            return;
        }
        if (i > 0)
            codebuf_write(params_sig, ", ");
        if (p->type != NULL)
            type_name = render_type_name_in_ctx(ctx, p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            char inner_buf[128];
            const char *inner = inner_buf;
            if (!slot_inner_type_name_copy(type_name, inner_buf,
                    sizeof(inner_buf))) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "cannot determine slot payload type for forward declaration '%s' parameter '%s'",
                    name != NULL ? name : "<function>",
                    p->name != NULL ? p->name : "<param>");
                free(type_name);
                if (params_sig != NULL)
                    codebuf_destroy(params_sig);
                free(header_decl);
                return;
            }
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE) {
            decl = pergyra_ast_typed_declarator(p->type, p->name);
            codebuf_write(params_sig, "%s", decl);
        } else if (p->name != NULL && strcmp(p->name, "self") != 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(type_name);
    }
    header_decl = pergyra_func_signature_declarator(return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(buf, "%s;\n", header_decl);
    free(header_decl);
    codebuf_destroy(params_sig);
}

void
emit_func_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    emit_func_forward_decl_named(node, ast_declaration_name(node), buf, ctx);
}
