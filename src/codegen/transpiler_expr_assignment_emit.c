#include "transpiler_expr_assignment_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../semantic/diag_codes.h"

#include "codegen_type_mapping.h"
#include "transpiler_context.h"
#include "transpiler_expr_dispatch_operand.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_overlay_projection.h"
#include "transpiler_slot_runtime_row.h"
#include "transpiler_symbols.h"

char *
transpiler_emit_assignment_expression_parts(TranspilerCtx *ctx,
                                            ASTNode *target_node,
                                            ASTNode *value_node)
{
    /*
     * MIR assignment emission captures target/value as instruction facts.
     * Keep the assignment expression policy here so the AST and MIR paths
     * share one C lowering owner without reopening the source statement node.
     */
    if (target_node != NULL && target_node->type == AST_ARRAY_ACCESS) {
        ASTNode *array_node = ast_array_access_array(target_node);
        ASTNode *index_node = ast_array_access_index(target_node);
        const char *array_type =
            array_node != NULL
                ? infer_expression_type_name(ctx, array_node)
                : NULL;
        if (array_type != NULL
            && transpiler_type_name_is_array(array_type)) {
            char inner_buf[128];
            const char *inner = NULL;
            if (slot_inner_type_name_copy(array_type, inner_buf,
                    sizeof(inner_buf))) {
                inner = inner_buf;
            }
            if (inner != NULL && inner[0] != '\0'
                && strcmp(inner, "Unknown") != 0) {
                char *array = transpiler_dispatch_emit_part(ctx,
                    array_node, "array assignment", "array");
                char *index = NULL;
                char *value = NULL;
                char *result;
                if (array == NULL)
                    return NULL;
                index = transpiler_dispatch_emit_part(ctx,
                    index_node, "array assignment", "index");
                if (index == NULL) {
                    free(array);
                    return NULL;
                }
                value = transpiler_dispatch_emit_part(ctx,
                    value_node, "array assignment", "value");
                if (value == NULL) {
                    free(array);
                    free(index);
                    return NULL;
                }
                result = strdup_fmt(
                    "pgy_array_set_%s(&%s, (size_t)(%s), %s)",
                    inner, array, index, value);
                free(array);
                free(index);
                free(value);
                return result;
            }
        }
        if (array_type != NULL
            && transpiler_type_name_is_slice(array_type)) {
            /* Write-through view: a Slice<T> is a fixed {data,len} span, so
             * setting through a copied view struct hits the same backing
             * storage -- mirrors the slice read emit's tmp-copy pattern. */
            char inner_buf[128];
            const char *inner = NULL;
            if (slot_inner_type_name_copy(array_type, inner_buf,
                    sizeof(inner_buf))) {
                inner = inner_buf;
            }
            if (inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C slice assignment requires concrete Slice<T> element metadata");
                return NULL;
            }
            char slice_suffix[128];
            sanitize_c_suffix(inner, slice_suffix, sizeof(slice_suffix));
            char *view = transpiler_dispatch_emit_part(ctx,
                array_node, "slice assignment", "slice");
            if (view == NULL)
                return NULL;
            char *index = transpiler_dispatch_emit_part(ctx,
                index_node, "slice assignment", "index");
            if (index == NULL) {
                free(view);
                return NULL;
            }
            char *value = transpiler_dispatch_emit_part(ctx,
                value_node, "slice assignment", "value");
            if (value == NULL) {
                free(view);
                free(index);
                return NULL;
            }
            int tmp_id = ++ctx->tmp_counter;
            char *result = strdup_fmt(
                "({ PgySlice_%s _pgy_slice_set_%d = %s; "
                "pgy_slice_set_%s(&_pgy_slice_set_%d, (size_t)(%s), %s); })",
                slice_suffix, tmp_id, view,
                slice_suffix, tmp_id, index, value);
            free(view);
            free(index);
            free(value);
            return result;
        }
    }
    if (target_node != NULL && target_node->type == AST_IDENTIFIER) {
        const char *tgt_name = ast_identifier_name(target_node);
        if (is_slot_var(ctx, tgt_name)) {
            char inner_buf[128];
            const char *inner = NULL;
            bool secure = lookup_slot_is_secure(ctx, tgt_name);
            char *value;
            char *slot_ref;
            char *result;
            if (lookup_slot_type_copy(ctx, tgt_name, inner_buf,
                    sizeof(inner_buf))) {
                inner = inner_buf;
            }
            if (inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "cannot determine slot payload type for assignment to '%s'",
                    tgt_name != NULL ? tgt_name : "<slot>");
                return NULL;
            }
            value = transpiler_dispatch_emit_part(ctx, value_node,
                "slot assignment", "value");
            if (value == NULL)
                return NULL;
            slot_ref = slot_ref_expr(ctx, tgt_name, tgt_name);
            if (slot_ref == NULL) {
                free(value);
                return NULL;
            }
            if (secure) {
                const char *write_fn;
                const char *token_name = require_slot_token_name(
                    ctx, tgt_name, "SecureSlot assignment");
                if (token_name == NULL) {
                    free(slot_ref);
                    free(value);
                    return NULL;
                }
                write_fn = transpiler_slot_runtime_fn(
                    ctx, true, inner, "Write");
                if (write_fn == NULL) {
                    free(slot_ref);
                    free(value);
                    return NULL;
                }
                result = strdup_fmt("%s(%s, %s, &%s)",
                    write_fn, slot_ref, value, token_name);
            } else {
                const char *write_fn = transpiler_slot_runtime_fn(
                    ctx, false, inner, "Write");
                if (write_fn == NULL) {
                    free(slot_ref);
                    free(value);
                    return NULL;
                }
                result = strdup_fmt("%s(%s, %s)",
                    write_fn, slot_ref, value);
            }
            free(slot_ref);
            free(value);
            return result;
        }
    }
    {
        char *target = transpiler_dispatch_emit_part(ctx, target_node,
            "assignment", "target");
        char *value = NULL;
        char *invalidation;
        char *post_sync;
        char *result;
        if (target == NULL)
            return NULL;
        value = transpiler_dispatch_emit_part(ctx, value_node,
            "assignment", "value");
        if (value == NULL) {
            free(target);
            return NULL;
        }
        invalidation = emit_assignment_projection_invalidation(
            ctx, target_node);
        post_sync = emit_world_embedded_assignment_sync(ctx, target_node);
        if (post_sync != NULL) {
            result = strdup_fmt("({ %s%s = %s; %s%s; })",
                invalidation != NULL ? invalidation : "",
                target, value, post_sync, target);
        } else if (invalidation != NULL) {
            result = strdup_fmt("({ %s%s = %s; })",
                invalidation, target, value);
        } else {
            result = strdup_fmt("%s = %s", target, value);
        }
        free(invalidation);
        free(post_sync);
        free(target);
        free(value);
        return result;
    }
}
