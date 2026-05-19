#include "transpiler_expr_composite_literal_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_type_mapping.h"

static char *
emit_tuple_literal_expression(ASTNode *node, TranspilerCtx *ctx)
{
    char tuple_name_buf[256];
    const char *tuple_name = NULL;
    if (ctx->expected_type != NULL && ctx->expected_type[0] == '(')
        tuple_name = ctx->expected_type;
    else if (ctx->current_return_type[0] == '(')
        tuple_name = ctx->current_return_type;
    if (tuple_name == NULL) {
        size_t off = 0;
        tuple_name_buf[0] = '\0';
        off = pergyra_str_append(tuple_name_buf, sizeof(tuple_name_buf), "(");
        for (size_t i = 0; i < ast_tuple_literal_count(node); i++) {
            const char *et = infer_expression_type_name(ctx,
                ast_tuple_literal_element(node, i));
            if (et == NULL || et[0] == '\0'
                || strcmp(et, "Unknown") == 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: tuple literal requires concrete element type metadata");
                return pergyra_strdup("0");
            }
            if (i > 0) {
                off = pergyra_str_append(tuple_name_buf,
                    sizeof(tuple_name_buf), ", ");
            }
            off = pergyra_str_append(tuple_name_buf,
                sizeof(tuple_name_buf), et);
        }
        (void)off;
        (void)pergyra_str_append(tuple_name_buf, sizeof(tuple_name_buf), ")");
        tuple_name = tuple_name_buf;
    }
    char ctype_buf[256];
    const char *ctype = NULL;
    if (pergyra_type_to_c_copy(tuple_name, ctype_buf, sizeof(ctype_buf)))
        ctype = ctype_buf;
    if (ctype == NULL || ctype[0] == '\0'
        || strcmp(ctype, "Unknown") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C tuple literal requires concrete tuple layout metadata");
        return pergyra_strdup("0");
    }
    CodeBuf *out = codebuf_create();
    codebuf_write(out, "((%s){", ctype);
    for (size_t i = 0; i < ast_tuple_literal_count(node); i++) {
        char *v = emit_expression(ast_tuple_literal_element(node, i), ctx);
        if (i > 0)
            codebuf_write(out, ", ");
        codebuf_write(out, ".f%zu = %s", i, v);
        free(v);
    }
    codebuf_write(out, "})");
    char *result = pergyra_strdup(out->data);
    codebuf_destroy(out);
    return result;
}

static char *
emit_array_literal_expression(ASTNode *node, TranspilerCtx *ctx)
{
    const char *array_type = infer_expression_type_name(ctx, node);
    char inner_buf[128];
    const char *inner = NULL;
    if (slot_inner_type_name_copy(array_type, inner_buf, sizeof(inner_buf)))
        inner = inner_buf;
    if (array_type == NULL || strncmp(array_type, "Array<", 6) != 0
        || inner == NULL || inner[0] == '\0'
        || strcmp(inner, "Unknown") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C array literal requires concrete Array<T> element metadata");
        return pergyra_strdup("0");
    }
    int tmp_id = ++ctx->tmp_counter;
    CodeBuf *buf = codebuf_create();
    codebuf_write(buf, "({ PgyArray_%s _pgy_arr_%d = pgy_array_new_%s(%zu); ",
        inner, tmp_id, inner, ast_array_literal_count(node));
    for (size_t i = 0; i < ast_array_literal_count(node); i++) {
        char *elem = emit_expression(ast_array_literal_element(node, i), ctx);
        codebuf_write(buf, "pgy_array_push_%s(&_pgy_arr_%d, %s); ",
            inner, tmp_id, elem);
        free(elem);
    }
    codebuf_write(buf, "_pgy_arr_%d; })", tmp_id);
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

char *
emit_composite_literal_expression(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return pergyra_strdup("0");
    if (node->type == AST_TUPLE_LITERAL)
        return emit_tuple_literal_expression(node, ctx);
    if (node->type == AST_ARRAY_LITERAL)
        return emit_array_literal_expression(node, ctx);
    return pergyra_strdup("0");
}
