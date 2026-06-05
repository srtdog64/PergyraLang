#include "transpiler_expr_dispatch_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

#include "codegen_match_variant_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_enum.h"
#include "transpiler_expr_array_access_emit.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_expr_call_spawn_emit.h"
#include "transpiler_expr_composite_literal_emit.h"
#include "transpiler_expr_core_emit.h"
#include "transpiler_expr_literal_emit.h"
#include "transpiler_expr_party_instance_emit.h"
#include "transpiler_format.h"
#include "transpiler_future_type_query.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_option_context.h"
#include "transpiler_overlay_host_fields.h"
#include "transpiler_overlay_projection.h"
#include "transpiler_projection.h"
#include "transpiler_slot_target.h"
#include "transpiler_spawn_channel_emit.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_require.h"

static char *
transpiler_dispatch_emit_part(TranspilerCtx *ctx,
                              ASTNode *expr,
                              const char *owner,
                              const char *role)
{
    char *rendered = emit_expression(expr, ctx);

    if (rendered != NULL)
        return rendered;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: %s could not lower %s expression",
        owner != NULL ? owner : "expression",
        role != NULL ? role : "operand");
    return NULL;
}

char *
emit_expression(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: expression lowering received a null AST node");
        return NULL;
    }

    switch (node->type) {
    case AST_NUMBER:
    case AST_STRING:
    case AST_BOOLEAN:
        return emit_literal_expression(node);

    case AST_IDENTIFIER: {
        const char *id_name = ast_identifier_name(node);
        if (id_name == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: identifier expression is missing a name");
            return NULL;
        }
        /* None is target-typed; without contextual Option<T> semantic should
         * already reject it, and the backend keeps a hard guard. */
        if (pgy_codegen_match_variant_lookup(id_name)
                == PGY_MATCH_VARIANT_NONE_CTOR) {
            return transpiler_emit_none_with_context(ctx, node);
        }
        /* Inside parallel wrapper: captured outer variables are accessed
         * through the context struct pointer.  (*_pctx->x) yields the
         * value, and &(*_pctx->x) collapses to _pctx->x (a pointer). */
        if (ctx->in_parallel_wrapper) {
            for (int i = 0; i < ctx->par_capture_slot_count; i++) {
                if (strcmp(ctx->par_capture_slot_names[i], id_name) == 0)
                    return strdup_fmt("(*_pctx->%s)", id_name);
            }
            for (int i = 0; i < ctx->par_capture_typed_count; i++) {
                if (strcmp(ctx->par_capture_typed_names[i], id_name) == 0)
                    return strdup_fmt("(*_pctx->%s)", id_name);
            }
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_class_has_field(ctx, id_name)) {
            return strdup_fmt(current_class_uses_self_cell(ctx)
                ? "self->%s"
                : "self.%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && (current_party_has_field(ctx, id_name)
                || current_roster_has_field(ctx, id_name))) {
            return strdup_fmt("self->%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_relation_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_effect_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_zone_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && transpiler_current_world_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        const char *ssa_name = transpiler_resolve_active_ssa_name(ctx, id_name);
        if (ssa_name != NULL) {
            char *c_ssa_name = transpiler_make_c_ssa_name(ctx, ssa_name);
            const char *slot_type = lookup_typed_var(ctx, id_name);
            if (slot_type == NULL) {
                char base_name[128];
                size_t version = 0;
                if (transpiler_parse_versioned_name(id_name, base_name, sizeof(base_name), &version))
                    slot_type = lookup_typed_var(ctx, base_name);
            }
            if (slot_type != NULL && transpiler_type_name_is_slot_like(slot_type)
                && !ctx->suppress_slot_auto_read) {
                char inner_buf[128];
                const char *inner = NULL;
                bool secure = strncmp(slot_type, "SecureSlot<", 11) == 0;
                if (slot_inner_type_name_copy(slot_type, inner_buf,
                        sizeof(inner_buf)))
                    inner = inner_buf;
                if (inner == NULL || inner[0] == '\0'
                    || strcmp(inner, "Unknown") == 0) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C slot SSA auto-read requires concrete Slot<T> payload metadata");
                    free(c_ssa_name);
                    return NULL;
                }
                const char *token_name = secure
                    ? require_slot_token_name(ctx, id_name, "SecureSlot SSA auto-read")
                    : NULL;
                if (secure && token_name == NULL) {
                    free(c_ssa_name);
                    return NULL;
                }
                char *result = secure
                    ? strdup_fmt("pgy_secure_read_%s(&%s, &%s)",
                                  inner, c_ssa_name, token_name)
                    : strdup_fmt("pgy_read_%s(&%s)", inner, c_ssa_name);
                free(c_ssa_name);
                return result;
            }
            return c_ssa_name;
        }
        {
            ASTNode *alias_expr = lookup_alias_expr(ctx, id_name);
            if (alias_expr != NULL)
                return emit_expression(alias_expr, ctx);
        }
        TypedVarEntry *projection_entry = lookup_typed_entry(ctx, id_name);
        /* Slot sugar: auto-Read emits pgy_read_T(&x) instead of x. */
        if (!ctx->suppress_slot_auto_read && is_slot_var(ctx, id_name)) {
            char inner_buf[128];
            const char *inner = NULL;
            bool secure = lookup_slot_is_secure(ctx, id_name);
            if (lookup_slot_type_copy(ctx, id_name, inner_buf,
                    sizeof(inner_buf))) {
                inner = inner_buf;
            }
            if (inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for auto-read of '%s'",
                    id_name != NULL ? id_name : "<slot>");
                return NULL;
            }
            char *slot_ref = slot_ref_expr(ctx, id_name, id_name);
            if (slot_ref == NULL)
                return NULL;
            const char *token_name = secure
                ? require_slot_token_name(ctx, id_name, "SecureSlot auto-read")
                : NULL;
            if (secure && token_name == NULL) {
                free(slot_ref);
                return NULL;
            }
            char *result = secure
                ? strdup_fmt("pgy_secure_read_%s(%s, &%s)",
                              inner, slot_ref, token_name)
                : strdup_fmt("pgy_read_%s(%s)", inner, slot_ref);
            free(slot_ref);
            return result;
        }
        {
            char enum_variant[128];
            if (lookup_enum_variant_qualified_name_copy(ctx, id_name,
                    enum_variant, sizeof(enum_variant))) {
                return pergyra_strdup(enum_variant);
            }
        }
        if (projection_entry != NULL && projection_entry->is_projection_borrow) {
            const char *source_type = lookup_typed_var(ctx, projection_entry->source_slot);
            ASTNode *target_decl = transpiler_find_projection_nominal_decl_local(
                ctx, projection_entry->type_name);
            ASTNode *source_decl = source_type != NULL
                ? transpiler_find_projection_nominal_decl_local(ctx, source_type)
                : NULL;
            if (target_decl != NULL && source_decl != NULL) {
                return emit_projection_literal(ctx, target_decl, source_decl,
                    NULL, projection_entry->type_name, projection_entry->source_slot);
            }
        }
        return pergyra_strdup(id_name);
    }

    case AST_BINARY:
        return emit_binary(node, ctx);

    case AST_UNARY:
        return emit_unary(node, ctx);

    case AST_CALL:
        return emit_call(node, ctx);

    case AST_MEMBER_ACCESS: {
        ASTNode *member_object = ast_member_object(node);
        const char *member_name = ast_member_name(node);
        if (member_object != NULL
            && member_object->type == AST_IDENTIFIER
            && member_name != NULL) {
            TypedVarEntry *entry = lookup_typed_entry(ctx,
                ast_identifier_name(member_object));
            if (entry != NULL && entry->is_projection_borrow) {
                const char *source_type = lookup_typed_var(ctx, entry->source_slot);
                ASTNode *source_decl = source_type != NULL
                    ? transpiler_find_projection_nominal_decl_local(ctx, source_type)
                    : NULL;
                char *source_path = NULL;
                int source_status = 0;
                if (source_decl != NULL) {
                    source_status = resolve_projection_source_path_rec(
                        ctx, source_decl, member_name, 0, &source_path);
                }
                if (source_status == 1 && source_path != NULL) {
                    TypedVarEntry *source_entry = lookup_typed_entry(ctx, entry->source_slot);
                    char *result = strdup_fmt(
                        (source_entry != NULL && source_entry->is_subject_ref)
                            ? "%s->%s"
                            : "%s.%s",
                        entry->source_slot, source_path);
                    return result;
                }
            }
        }
        char *obj = transpiler_dispatch_emit_part(ctx, member_object,
            "member access", "receiver");
        if (obj == NULL)
            return NULL;
        /* Enum variant access: Color.Red -> Color_Red. */
        if (member_object->type == AST_IDENTIFIER
            && ast_identifier_name(member_object) != NULL
            && ast_identifier_name(member_object)[0] >= 'A'
            && ast_identifier_name(member_object)[0] <= 'Z') {
            char *result = strdup_fmt("%s_%s", obj, member_name);
            free(obj);
            return result;
        }
        if (member_object->type == AST_IDENTIFIER
            && strcmp(ast_identifier_name(member_object), "self") == 0) {
            ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
            bool self_is_pointer = current_class_uses_self_cell(ctx)
                || transpiler_host_decl_uses_pointer_self(host_decl);
            char *result = strdup_fmt(self_is_pointer
                ? "%s->%s"
                : "%s.%s", obj, member_name);
            free(obj);
            return result;
        }
        /* Subject-ref parameter: use -> for member access */
        if (member_object->type == AST_IDENTIFIER) {
            TypedVarEntry *entry = lookup_typed_entry(ctx,
                ast_identifier_name(member_object));
            if (entry != NULL && entry->is_subject_ref) {
                char *result = strdup_fmt("%s->%s", obj, member_name);
                free(obj);
                return result;
            }
        }
        char *result = strdup_fmt("%s.%s", obj, member_name);
        free(obj);
        return result;
    }

    case AST_ARRAY_ACCESS: {
        return emit_array_access_expression(node, ctx);
    }

    case AST_TUPLE_LITERAL:
    case AST_ARRAY_LITERAL:
        return emit_composite_literal_expression(node, ctx);

    case AST_ASSIGNMENT: {
        ASTNode *target_node = ast_assignment_target(node);
        ASTNode *value_node = ast_assignment_value(node);
        /*
         * Array element assignment sugar: arr[i] = val -> pgy_array_set_T(&arr, i, val).
         * The default emit path renders arr[i] as a compound `({ ... pgy_array_get_T(...); })`
         * expression which is not a valid C lvalue. The set helper accepts the
         * array by pointer and performs the bounds-checked write.
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
                    char *result = strdup_fmt(
                        "pgy_array_set_%s(&%s, (size_t)(%s), %s)",
                        inner, array, index, value);
                    free(array);
                    free(index);
                    free(value);
                    return result;
                }
            }
        }
        /* Slot sugar: x = 5 -> pgy_write_T(&x, 5). */
        if (target_node != NULL && target_node->type == AST_IDENTIFIER) {
            const char *tgt_name = ast_identifier_name(target_node);
            if (is_slot_var(ctx, tgt_name)) {
                char inner_buf[128];
                const char *inner = NULL;
                bool secure = lookup_slot_is_secure(ctx, tgt_name);
                if (lookup_slot_type_copy(ctx, tgt_name, inner_buf,
                        sizeof(inner_buf))) {
                    inner = inner_buf;
                }
                if (inner == NULL || inner[0] == '\0'
                    || strcmp(inner, "Unknown") == 0) {
                    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for assignment to '%s'",
                        tgt_name != NULL ? tgt_name : "<slot>");
                    return NULL;
                }
                char *value = transpiler_dispatch_emit_part(ctx, value_node,
                    "slot assignment", "value");
                if (value == NULL)
                    return NULL;
                char *slot_ref = slot_ref_expr(ctx, tgt_name, tgt_name);
                if (slot_ref == NULL) {
                    free(value);
                    return NULL;
                }
                char *result;
                if (secure) {
                    const char *token_name = require_slot_token_name(
                        ctx, tgt_name, "SecureSlot assignment");
                    if (token_name == NULL) {
                        free(slot_ref);
                        free(value);
                        return NULL;
                    }
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s)",
                        inner, slot_ref, value, token_name);
                } else {
                    result = strdup_fmt("pgy_write_%s(%s, %s)",
                        inner, slot_ref, value);
                }
                free(slot_ref);
                free(value);
                return result;
            }
        }
        char *target = transpiler_dispatch_emit_part(ctx, target_node,
            "assignment", "target");
        char *value = NULL;
        if (target == NULL)
            return NULL;
        value = transpiler_dispatch_emit_part(ctx, value_node,
            "assignment", "value");
        if (value == NULL) {
            free(target);
            return NULL;
        }
        char *invalidation = emit_assignment_projection_invalidation(
            ctx, target_node);
        char *post_sync = emit_world_embedded_assignment_sync(
            ctx, target_node);
        char *result;
        if (post_sync != NULL)
            result = strdup_fmt("({ %s%s = %s; %s%s; })",
                invalidation != NULL ? invalidation : "",
                target, value, post_sync, target);
        else if (invalidation != NULL)
            result = strdup_fmt("({ %s%s = %s; })", invalidation, target, value);
        else
            result = strdup_fmt("%s = %s", target, value);
        free(invalidation);
        free(post_sync);
        free(target);
        free(value);
        return result;
    }

    case AST_AWAIT_EXPR:
        {
            ASTNode *awaited = ast_await_expression(node);
            char *expr = transpiler_dispatch_emit_part(ctx, awaited,
                "await", "task handle");
            char inner_buf[128];
            const char *inner = NULL;
            bool is_remote = is_remote_future_expr(ctx,
                awaited);
            char *result;
            if (lookup_future_inner_type_copy(ctx,
                    awaited,
                    inner_buf, sizeof(inner_buf))) {
                inner = inner_buf;
            }
            if (expr == NULL
                || inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C await expression requires concrete Future<T> result metadata");
                free(expr);
                return NULL;
            }
            if (is_remote && strcmp(inner, "Void") == 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C await expression does not lower RemoteFuture<Void>; "
                    "semantic analysis must reject it until Result<Void> ABI "
                    "is frozen");
                free(expr);
                return NULL;
            }
            if (strcmp(inner, "Void") == 0) {
                result = strdup_fmt("pgy_await_void(%s)", expr);
            } else if (is_remote) {
                /* RemoteFuture<T> -> Result<T>: wrap in PgyResult. */
                char inner_c_type_buf[256];
                const char *inner_c_type = NULL;
                if (transpiler_require_type_name_c_type_copy(ctx, inner,
                        "RemoteFuture await result",
                        inner_c_type_buf,
                        sizeof(inner_c_type_buf))) {
                    inner_c_type = inner_c_type_buf;
                }
                if (inner_c_type == NULL) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C await expression cannot render RemoteFuture result type '%s'",
                        inner);
                    free(expr);
                    return NULL;
                }
                result = strdup_fmt("pgy_await_result_take(%s, %s, %s)",
                    expr, inner, inner_c_type);
            } else {
                char inner_c_type_buf[256];
                const char *inner_c_type = NULL;
                if (transpiler_require_type_name_c_type_copy(ctx, inner,
                        "Future await result",
                        inner_c_type_buf,
                        sizeof(inner_c_type_buf))) {
                    inner_c_type = inner_c_type_buf;
                }
                if (inner_c_type == NULL) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C await expression cannot render Future result type '%s'",
                        inner);
                    free(expr);
                    return NULL;
                }
                result = strdup_fmt("pgy_await_take(%s, %s)",
                    expr, inner_c_type);
            }
            free(expr);
            return result;
        }

    case AST_SPAWN_EXPR:
        return emit_spawn_expr(node, ctx);

    case AST_CHANNEL_SEND:
        return emit_channel_send(node, ctx);

    case AST_CHANNEL_RECV:
        return emit_channel_recv(node, ctx);

    case AST_EVENT_INVOKE: {
        char *event = transpiler_dispatch_emit_part(ctx,
            ast_event_invoke_event(node), "event invoke", "event");
        CodeBuf *args = codebuf_create();
        if (event == NULL) {
            codebuf_destroy(args);
            return NULL;
        }
        for (size_t i = 0; i < ast_event_invoke_arg_count(node); i++) {
            char *arg = transpiler_dispatch_emit_part(ctx,
                ast_event_invoke_argument(node, i),
                "event invoke", "argument");
            if (arg == NULL) {
                free(event);
                codebuf_destroy(args);
                return NULL;
            }
            if (i > 0)
                codebuf_write(args, ", ");
            codebuf_write(args, "%s", arg);
            free(arg);
        }
        char *result = strdup_fmt("%s_INVOKE(&%s%s%s)",
                                  event,
                                  event,
                                  args->len > 0 ? ", " : "",
                                  args->data);
        free(event);
        codebuf_destroy(args);
        return result;
    }

    case AST_CONTEXT_ACCESS:
        if (ast_context_access_role_slot_name(node) != NULL) {
            return strdup_fmt("self->%s",
                              ast_context_access_role_slot_name(node));
        }
        return pergyra_strdup("self");

    case AST_PARTY_INSTANCE:
        return emit_party_instance_expr(node, ctx);

    case AST_LAMBDA_EXPR:
        return emit_lambda_expr(node, ctx);

    default:
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported expression node type %d at line %d",
            (int)node->type, node->line);
        return NULL;
    }
}
