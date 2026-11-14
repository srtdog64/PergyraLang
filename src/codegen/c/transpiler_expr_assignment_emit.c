#include "transpiler_expr_assignment_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../../semantic/diag_codes.h"

#include "../codegen_type_mapping.h"
#include "transpiler_context.h"
#include "transpiler_expr_dispatch_operand.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_expr_core_emit.h"
#include "transpiler_format.h"
#include "transpiler_overlay_projection.h"
#include "transpiler_slot_runtime_row.h"
#include "transpiler_symbols.h"

/* All names in a captured place share an identity. Reserve the entire group
 * against user bindings and rendered subexpressions, including nested writes. */
static int
assignment_capture_identity(TranspilerCtx *ctx, const char *target,
                            const char *index, const char *value)
{
    const char *prefixes[] = {
        "_pgy_update_array_", "_pgy_update_index_", "_pgy_update_value_",
        "_pgy_slice_set_", "_pgy_assign_target_", "_pgy_assign_value_"
    };
    for (;;) {
        int id = ++ctx->tmp_counter;
        bool available = true;
        for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
            char name[80];
            snprintf(name, sizeof(name), "%s%d", prefixes[i], id);
            if (lookup_typed_entry(ctx, name) != NULL
                || (target != NULL && strstr(target, name) != NULL)
                || (index != NULL && strstr(index, name) != NULL)
                || (value != NULL && strstr(value, name) != NULL)) {
                available = false;
                break;
            }
        }
        if (available)
            return id;
    }
}

char *
transpiler_emit_assignment_expression_parts(TranspilerCtx *ctx,
                                            ASTNode *target_node,
                                            ASTNode *value_node,
                                            AstAssignmentOperation operation)
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
                if (operation == AST_ASSIGN_SET) {
                    /* Runtime kernels are named by the sanitized element
                     * suffix: an Array<String> element is Array_String. */
                    char suffix[128];
                    sanitize_c_suffix(inner, suffix, sizeof(suffix));
                    result = strdup_fmt(
                        "pgy_array_set_%s(&%s, (size_t)(%s), %s)",
                        suffix, array, index, value);
                } else {
                    int id = assignment_capture_identity(ctx, array, index, value);
                    char *old = strdup_fmt("pgy_array_get_%s(_pgy_update_array_%d, _pgy_update_index_%d)",
                        inner, id, id);
                    char *updated = transpiler_sequence_scalar_operation(ctx,
                        ast_assignment_binary_operator(operation), inner, old, value, value_node);
                    result = updated == NULL ? NULL : strdup_fmt(
                        "({ __auto_type _pgy_update_array_%d = &(%s); "
                        "size_t _pgy_update_index_%d = (size_t)(%s); "
                        "__auto_type _pgy_update_value_%d = (%s); "
                        "pgy_array_set_%s(_pgy_update_array_%d, _pgy_update_index_%d, _pgy_update_value_%d); "
                        "_pgy_update_value_%d; })", id, array, id, index, id, updated,
                        inner, id, id, id, id);
                    free(old);
                    free(updated);
                }
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
            int tmp_id = assignment_capture_identity(ctx, view, index, value);
            char *result;
            if (operation == AST_ASSIGN_SET) {
                result = strdup_fmt(
                    "({ PgySlice_%s _pgy_slice_set_%d = %s; "
                    "pgy_slice_set_%s(&_pgy_slice_set_%d, (size_t)(%s), %s); })",
                    slice_suffix, tmp_id, view,
                    slice_suffix, tmp_id, index, value);
            } else {
                char *old = strdup_fmt("pgy_slice_get_%s(&_pgy_slice_set_%d, _pgy_update_index_%d)",
                    slice_suffix, tmp_id, tmp_id);
                char *updated = transpiler_sequence_scalar_operation(ctx,
                    ast_assignment_binary_operator(operation), inner, old, value, value_node);
                result = updated == NULL ? NULL : strdup_fmt(
                    "({ PgySlice_%s _pgy_slice_set_%d = (%s); "
                    "size_t _pgy_update_index_%d = (size_t)(%s); "
                    "__auto_type _pgy_update_value_%d = (%s); "
                    "pgy_slice_set_%s(&_pgy_slice_set_%d, _pgy_update_index_%d, _pgy_update_value_%d); "
                    "_pgy_update_value_%d; })", slice_suffix, tmp_id, view, tmp_id, index,
                    tmp_id, updated, slice_suffix, tmp_id, tmp_id, tmp_id, tmp_id);
                free(old);
                free(updated);
            }
            free(view);
            free(index);
            free(value);
            return result;
        }
    }
    if (target_node != NULL && target_node->type == AST_IDENTIFIER) {
        const char *tgt_name = ast_identifier_name(target_node);
        if (is_slot_var(ctx, tgt_name)) {
            if (operation != AST_ASSIGN_SET) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "Compound assignment requires a numeric value, not a slot handle");
                return NULL;
            }
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
        /* LLVM fixes the target before evaluating the RHS. Capture its
         * address once as well: a member target may contain an effectful
         * index, and reading the assignment result must not run it again. */
        int target_id = assignment_capture_identity(ctx, target, NULL, value);
        if (operation != AST_ASSIGN_SET) {
            char *old = strdup_fmt("*_pgy_assign_target_%d", target_id);
            char *updated = transpiler_sequence_scalar_operation(ctx,
                ast_assignment_binary_operator(operation),
                infer_expression_type_name(ctx, target_node), old, value, value_node);
            free(old);
            free(value);
            value = updated;
            if (value == NULL) {
                free(invalidation); free(post_sync); free(target);
                return NULL;
            }
        }
        result = strdup_fmt(
            "({ %s__auto_type _pgy_assign_target_%d = &(%s); "
            "__auto_type _pgy_assign_value_%d = (%s); "
            "*_pgy_assign_target_%d = _pgy_assign_value_%d; "
            "%s_pgy_assign_value_%d; })",
            invalidation != NULL ? invalidation : "", target_id, target,
            target_id, value, target_id, target_id,
            post_sync != NULL ? post_sync : "", target_id);
        free(invalidation);
        free(post_sync);
        free(target);
        free(value);
        return result;
    }
}
