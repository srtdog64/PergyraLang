#include <stdlib.h>
#include <string.h>

#include "transpiler_context.h"
#include "transpiler_destructure_emit.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_require.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

void
emit_let_destructure_statement(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *init = ast_let_destructure_initializer(node);
    char *init_expr = emit_expression(init, ctx);
    const char *init_type = transpiler_expr_infer_type_name(ctx, init);
    char c_init_type_buf[128];
    const char *c_init_type = NULL;
    int tmp_id;

    if ((init_type == NULL || strcmp(init_type, "Unknown") == 0)
        && init != NULL
        && init->type == AST_IDENTIFIER
        && ast_identifier_name(init) != NULL) {
        const char *resolved = lookup_typed_var(ctx, ast_identifier_name(init));
        if (resolved != NULL)
            init_type = resolved;
    }
    if (transpiler_require_type_name_c_type_copy(ctx, init_type,
            "destructuring initializer", c_init_type_buf,
            sizeof(c_init_type_buf))) {
        c_init_type = c_init_type_buf;
    }

    if (init_type != NULL && init_type[0] == '(') {
        char elem_names[8][64];
        size_t arity = 0;
        size_t i = 1;
        size_t n = strlen(init_type);

        while (i < n && init_type[i] != ')' && arity < 8) {
            size_t eo = 0;
            int depth = 0;

            while (i < n && (init_type[i] == ' ' || init_type[i] == '\t'))
                i++;
            while (i < n && eo + 1 < sizeof(elem_names[0])) {
                char c = init_type[i];
                if (depth == 0 && (c == ',' || c == ')'))
                    break;
                if (c == '<' || c == '(')
                    depth++;
                if (c == '>' || c == ')')
                    depth--;
                elem_names[arity][eo++] = c;
                i++;
            }
            elem_names[arity][eo] = '\0';
            while (eo > 0 && (elem_names[arity][eo - 1] == ' '
                           || elem_names[arity][eo - 1] == '\t')) {
                elem_names[arity][--eo] = '\0';
            }
            arity++;
            if (i < n && init_type[i] == ',')
                i++;
        }

        if (arity != ast_let_destructure_name_count(node)) {
            transpiler_set_mir_topology_invalid(
                ctx,
                "tuple destructuring arity mismatch: binding %llu, tuple arity %llu",
                (unsigned long long)ast_let_destructure_name_count(node),
                (unsigned long long)arity);
            free(init_expr);
            return;
        }

        tmp_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s _pgy_destr_%d = %s;\n",
            c_init_type, tmp_id, init_expr);
        for (size_t j = 0; j < arity; j++) {
            char e_ctype_buf[256];
            const char *e_ctype = NULL;
            if (transpiler_require_type_name_c_type_copy(ctx, elem_names[j],
                    "tuple destructuring element", e_ctype_buf,
                    sizeof(e_ctype_buf))) {
                e_ctype = e_ctype_buf;
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = _pgy_destr_%d.f%zu;\n",
                e_ctype != NULL ? e_ctype : elem_names[j],
                ast_let_destructure_name(node, j), tmp_id, j);
            register_typed_var(ctx, ast_let_destructure_name(node, j),
                elem_names[j]);
        }
        free(init_expr);
        return;
    }

    const char *elem_c_type = NULL;
    const char *inner = NULL;
    char inner_buf[128];
    char elem_c_type_buf[128];
    if (init_type != NULL
        && (strncmp(init_type, "Array<", 6) == 0
            || strncmp(init_type, "Slice<", 6) == 0)) {
        if (slot_inner_type_name_copy(init_type, inner_buf,
                sizeof(inner_buf)))
            inner = inner_buf;
        if (transpiler_require_type_name_c_type_copy(ctx, inner,
                "array destructuring element", elem_c_type_buf,
                sizeof(elem_c_type_buf))) {
            elem_c_type = elem_c_type_buf;
        }
    }
    if (c_init_type == NULL || elem_c_type == NULL || inner == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot lower destructuring initializer of type '%s' to a concrete array element type",
            init_type != NULL ? init_type : "(unknown)");
        free(init_expr);
        return;
    }

    tmp_id = ++ctx->tmp_counter;
    write_indent(ctx);
    codebuf_write(ctx->out, "%s _pgy_destr_%d = %s;\n",
        c_init_type, tmp_id, init_expr);
    for (size_t i = 0; i < ast_let_destructure_name_count(node); i++) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = _pgy_destr_%d.data[%zu];\n",
            elem_c_type, ast_let_destructure_name(node, i), tmp_id, i);
        register_typed_var(ctx, ast_let_destructure_name(node, i), inner);
    }
    free(init_expr);
}
