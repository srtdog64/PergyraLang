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
#include "transpiler_type_require.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_collection_runtime_suffix.h"
#include "codegen_hashmap_key_policy.h"

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
                return NULL;
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
    if (transpiler_require_type_name_c_type_copy(ctx, tuple_name,
            "tuple literal layout", ctype_buf, sizeof(ctype_buf)))
        ctype = ctype_buf;
    if (ctype == NULL || ctype[0] == '\0'
        || strcmp(ctype, "Unknown") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C tuple literal requires concrete tuple layout metadata");
        return NULL;
    }
    CodeBuf *out = codebuf_create();
    codebuf_write(out, "((%s){", ctype);
    for (size_t i = 0; i < ast_tuple_literal_count(node); i++) {
        char *v = emit_expression(ast_tuple_literal_element(node, i), ctx);
        if (v == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C tuple literal could not lower element %zu", i);
            codebuf_destroy(out);
            return NULL;
        }
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
    if (!transpiler_type_name_is_array(array_type)
        || inner == NULL || inner[0] == '\0'
        || strcmp(inner, "Unknown") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C array literal requires concrete Array<T> element metadata");
        return NULL;
    }
    int tmp_id = ++ctx->tmp_counter;
    CodeBuf *buf = codebuf_create();
    codebuf_write(buf, "({ PgyArray_%s _pgy_arr_%d = pgy_array_new_%s(%zu); ",
        inner, tmp_id, inner, ast_array_literal_count(node));
    for (size_t i = 0; i < ast_array_literal_count(node); i++) {
        char *elem = emit_expression(ast_array_literal_element(node, i), ctx);
        if (elem == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C array literal could not lower element %zu", i);
            codebuf_destroy(buf);
            return NULL;
        }
        codebuf_write(buf, "pgy_array_push_%s(&_pgy_arr_%d, %s); ",
            inner, tmp_id, elem);
        free(elem);
    }
    codebuf_write(buf, "_pgy_arr_%d; })", tmp_id);
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static bool
map_literal_resolve_policy(TranspilerCtx *ctx, const char *map_type,
                          char *suffix_buf, size_t suffix_size,
                          const char **key_infix_out)
{
    char key_buf[64];
    char value_buf[64];
    const char *key = NULL;
    const char *value = NULL;

    if (!transpiler_require_hashmap_type(ctx, map_type, "map literal",
            key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
            &key, &value))
        return false;
    *key_infix_out = pgy_hashmap_key_c_infix(key);
    if (*key_infix_out == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: map literal requires HashMap<Bool|Int|Long|String, T> key metadata");
        return false;
    }
    transpiler_collection_ensure_specialization(ctx, "Map", value);
    collection_runtime_suffix_copy(value, suffix_buf, suffix_size);
    return true;
}

static char *
emit_map_literal_expression(ASTNode *node, TranspilerCtx *ctx)
{
    const char *map_type = infer_expression_type_name(ctx, node);
    const char *key_infix = NULL;
    char suffix_buf[128];
    char ctype_buf[256];
    int tmp_id;
    CodeBuf *buf;

    if (!map_literal_resolve_policy(ctx, map_type, suffix_buf,
            sizeof(suffix_buf), &key_infix))
        return NULL;
    if (!transpiler_require_type_name_c_type_copy(ctx, map_type,
            "map literal layout", ctype_buf, sizeof(ctype_buf))) {
        return NULL;
    }
    tmp_id = ++ctx->tmp_counter;
    buf = codebuf_create();
    codebuf_write(buf, "({ %s _pgy_map_%d = pgy_map_new_%s(); ",
        ctype_buf, tmp_id, suffix_buf);
    for (size_t i = 0; i < ast_map_literal_count(node); i++) {
        char *k = emit_expression(ast_map_literal_key(node, i), ctx);
        char *v = k != NULL
            ? emit_expression(ast_map_literal_value(node, i), ctx)
            : NULL;
        if (k == NULL || v == NULL) {
            free(k);
            free(v);
            codebuf_destroy(buf);
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C map literal could not lower entry %zu", i);
            return NULL;
        }
        codebuf_write(buf, "pgy_map_set%s_%s(&_pgy_map_%d, %s, %s); ",
            key_infix, suffix_buf, tmp_id, k, v);
        free(k);
        free(v);
    }
    codebuf_write(buf, "_pgy_map_%d; })", tmp_id);
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

char *
emit_composite_literal_expression(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return NULL;
    if (node->type == AST_TUPLE_LITERAL)
        return emit_tuple_literal_expression(node, ctx);
    if (node->type == AST_ARRAY_LITERAL)
        return emit_array_literal_expression(node, ctx);
    if (node->type == AST_MAP_LITERAL)
        return emit_map_literal_expression(node, ctx);
    return NULL;
}
