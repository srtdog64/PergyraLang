#include "transpiler_mir_inventory_intent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_signature.h"
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
    const MIRRoutine *mir_routine = transpiler_find_mir_function(ctx, node);
    bool allow_ast_compat = false;
    bool generic_func =
        transpiler_mir_or_ast_function_is_generic(mir_routine, node);
    if (transpiler_active_has_mir(ctx) && mir_routine == NULL
        && !generic_func) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing function forward routine for '%s'",
            name != NULL ? name : "(anonymous)");
        if (params_sig != NULL)
            codebuf_destroy(params_sig);
        return;
    }
    if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
            mir_routine,
            node,
            TRANSPILER_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES,
            "MIR-only C path missing function forward signature metadata for '%s'",
            "MIR-only C path missing function forward return type-name metadata for '%s'",
            "MIR-only C path missing function forward parameter type-name metadata for '%s'")) {
        if (params_sig != NULL)
            codebuf_destroy(params_sig);
        return;
    }
    allow_ast_compat = mir_routine == NULL;
    ASTNode *return_type;
    size_t param_count;
    if (allow_ast_compat) {
        return_type = ast_func_return_type(node);
        param_count = ast_func_param_count(node);
    } else {
        return_type = transpiler_mir_routine_return_type(mir_routine);
        param_count = transpiler_mir_routine_param_count(mir_routine);
    }
    ensure_type_specializations_from_ast(ctx, return_type);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *p;
        const char *pt = NULL;
        char pt_buf[256];
        const char *type_name;
        char *owned_type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        bool event_handler_param = false;

        if (allow_ast_compat) {
            p = ast_func_param(node, i);
            type_name = NULL;
        } else {
            p = transpiler_mir_routine_param(mir_routine, i);
            type_name = transpiler_mir_routine_param_type_name(mir_routine, i);
        }
        if (p == NULL)
            continue;
        if (p->type != NULL)
            ensure_type_specializations_from_ast(ctx, p->type);
        event_handler_param =
            p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE;
        if (!event_handler_param && type_name != NULL) {
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "forward declaration parameter '%s' of '%s'",
                p->name != NULL ? p->name : "(anonymous)",
                name != NULL ? name : "(anonymous)");
            if (transpiler_require_type_name_c_type_copy(ctx, type_name,
                    surface_desc, pt_buf, sizeof(pt_buf))) {
                pt = pt_buf;
            }
        } else if (!event_handler_param && p->type != NULL) {
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
        if (!event_handler_param && pt == NULL) {
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
        if (type_name == NULL && p->type != NULL) {
            owned_type_name = render_type_name_in_ctx(ctx, p->type);
            type_name = owned_type_name;
        }
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (p->mode == PARAM_MODE_MUT_REF) {
            codebuf_write(params_sig, "%s *%s__mutref", pt, p->name);
        } else if (boundary_slot) {
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
                free(owned_type_name);
                if (params_sig != NULL)
                    codebuf_destroy(params_sig);
                free(header_decl);
                return;
            }
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (event_handler_param) {
            decl = pergyra_ast_typed_declarator_in_ctx(ctx, p->type, p->name);
            if (decl == NULL) {
                free(owned_type_name);
                if (params_sig != NULL)
                    codebuf_destroy(params_sig);
                free(header_decl);
                return;
            }
            codebuf_write(params_sig, "%s", decl);
        } else if (p->name != NULL && strcmp(p->name, "self") != 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(owned_type_name);
    }
    header_decl = pergyra_func_signature_declarator_in_ctx(ctx, return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    if (header_decl == NULL) {
        if (params_sig != NULL)
            codebuf_destroy(params_sig);
        return;
    }
    codebuf_write(buf, "%s;\n", header_decl);
    free(header_decl);
    codebuf_destroy(params_sig);
}

void
emit_func_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    emit_func_forward_decl_named(node, ast_declaration_name(node), buf, ctx);
}
