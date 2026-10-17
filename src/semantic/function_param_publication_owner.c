/* One all-typed publication boundary. Pending rows own diagnostic strings,
 * borrow compilation-owned AST/Type identity, and never retain scope Symbols. */
#include "function_param_publication_internal.h"
#include "typed_map_query_fact_owner.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_metadata_internal.h"
#include "type_checker_call_constructor_internal.h"
#include "type_checker_resolution_internal.h"
#include "type_checker_ownership_internal.h"
#include "slot_analyzer_internal.h"
#include "function_param_flow_summary_internal.h"
#include "callable_capability_inference.h"
#include "region_retention_summary.h"
#include "indexed_string_borrow_owner.h"
#include "array_storage_release_owner.h"
#include "../common/string_compat.h"
#include <stdlib.h>
#include <string.h>
bool publication_physical_scalar(const Type *type)
{
    return type != NULL && type->kind == TYPE_KIND_PRIMITIVE &&
        (type == TYPE_INT || type == TYPE_LONG || type == TYPE_FLOAT ||
         type == TYPE_DOUBLE || type == TYPE_BOOL || type == TYPE_DURATION);
}
bool semantic_param_publication_begin(SemanticContext *ctx)
{
    if (ctx == NULL || ctx->program_root == NULL || ctx->program_root->type != AST_PROGRAM ||
        ast_node_stable_id(ctx->program_root) == 0 || ctx->function_param_publications != NULL ||
        ctx->function_param_flow_summaries != NULL) return false;
    ctx->function_param_publications = calloc(1, sizeof(*ctx->function_param_publications));
    if (ctx->function_param_publications == NULL) return false;
    ctx->function_param_publications->pending = true;
    ctx->function_param_publications->program = ctx->program_root;
    ctx->function_param_publications->program_id = ast_node_stable_id(ctx->program_root);
    ctx->function_param_publications->statement_count = ast_program_statement_count(ctx->program_root);
    return true;
}
bool publication_generation_ready(const SemanticContext *ctx)
{
    if (ctx == NULL || ctx->function_param_publications == NULL) return false;
    const FunctionParamPublicationStore *store = ctx->function_param_publications;
    return ctx->program_root == store->program && store->program_id != 0 &&
        ast_node_stable_id(ctx->program_root) == store->program_id &&
        ast_program_statement_count(ctx->program_root) == store->statement_count;
}
bool semantic_param_publication_ready(const SemanticContext *ctx)
{ return semantic_param_publication_inputs_ready(ctx) && ctx->function_param_publications->finalized; }
PublishedNode *publication_node(FunctionParamPublicationStore *store,
    const ASTNode *node, bool insert)
{
    uint32_t id = ast_node_stable_id(node);
    if (store == NULL || node == NULL || id == 0) return NULL;
    if (insert && (store->node_capacity == 0 || store->node_count >= store->node_capacity / 2)) {
        size_t capacity = store->node_capacity == 0 ? 64 : store->node_capacity * 2;
        if (capacity < store->node_capacity || capacity > SIZE_MAX / sizeof(PublishedNode)) return NULL;
        PublishedNode *rows = calloc(capacity, sizeof(*rows));
        if (rows == NULL) return NULL;
        for (size_t i = 0; i < store->node_capacity; i++) if (store->nodes[i].id != 0) {
            size_t slot = (store->nodes[i].id * (size_t)2654435761u) & (capacity - 1);
            while (rows[slot].id != 0) slot = (slot + 1) & (capacity - 1);
            rows[slot] = store->nodes[i];
        }
        free(store->nodes); store->nodes = rows; store->node_capacity = capacity;
    }
    if (store->node_capacity == 0) return NULL;
    size_t slot = (id * (size_t)2654435761u) & (store->node_capacity - 1);
    while (store->nodes[slot].id != 0 && store->nodes[slot].id != id)
        slot = (slot + 1) & (store->node_capacity - 1);
    PublishedNode *row = &store->nodes[slot];
    if (row->id != 0) return row->node == node ? row : NULL;
    if (!insert) return NULL;
    row->id = id; row->node = node; store->node_count++; return row;
}
PublishedNode *publication_node_by_id(FunctionParamPublicationStore *store, uint32_t id)
{
    if (store == NULL || id == 0 || store->node_capacity == 0) return NULL;
    size_t slot = (id * (size_t)2654435761u) & (store->node_capacity - 1);
    while (store->nodes[slot].id != 0 && store->nodes[slot].id != id)
        slot = (slot + 1) & (store->node_capacity - 1);
    return store->nodes[slot].id == id ? &store->nodes[slot] : NULL;
}
bool semantic_param_publication_begin_value(SemanticContext *ctx, const ASTNode *value)
{
    if (!semantic_param_publication_pending(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, true);
    if (row == NULL) return false;
    semantic_param_publication_retire_value(row);
    return true;
}
bool semantic_param_publication_record_selected_callable(SemanticContext *ctx,
    ASTNode *call, ASTNode *decl)
{
    if (!semantic_param_publication_pending(ctx) || call == NULL ||
        call->type != AST_CALL || decl == NULL || decl->type != AST_FUNC_DECL ||
        ast_node_stable_id(decl) == 0 ||
        ast_call_semantic_callee_decl_id(call) != ast_node_stable_id(decl) ||
        !ast_call_semantic_callee_declared_callable(call)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, true);
    if (row == NULL || (row->selected_callable != NULL && row->selected_callable != decl)) return false;
    row->selected_callable = decl;
    row->selected_callable_id = ast_node_stable_id(decl);
    return true;
}
bool semantic_param_publication_record_value(SemanticContext *ctx, ASTNode *value,
    const Type *type, bool meta)
{
    if (!semantic_param_publication_pending(ctx) || type == NULL || type == TYPE_UNKNOWN ||
        (type->kind == TYPE_KIND_PRIMITIVE && type->name != NULL &&
         (strcmp(type->name, "Unknown") == 0 || strcmp(type->name, "<unknown>") == 0))) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, true);
    if (row == NULL) return false;
    if (row->value_type_name != NULL || row->nominal_result != NULL) {
        char *name = pergyra_strdup(type->name);
        if (name == NULL) return false;
        free(row->value_type_name); row->value_type_name = name;
    }
    /* Category owns source value semantics. Origin separately owns whether
     * this value can escape without a backing loan or fresh copy landing. */
    row->category = (uint32_t)semantic_classify_ownership_type(type, ctx);
    row->value_shape = value->type;
    row->string_value = type_equals(type, TYPE_STRING);
    row->value_type = type;
    bool copy = publication_physical_scalar(type);
    row->value_present = true; row->copy_escape = copy;
    row->physical_scalar = copy;
    row->string_array_type = type_is_constructed_named(type, "Array") &&
        type_equals(type_get_constructed_arg(type, 0), TYPE_STRING) ? type : NULL;
    row->origin_ready = copy || (row->string_value && row->builtin_string_concat);
    if (row->string_value && value->type == AST_STRING &&
        semantic_indexed_string_independent_result(value, ctx))
        row->origin_ready = row->copy_escape = true;
    row->binding_id = value->type == AST_IDENTIFIER ? ast_identifier_binding_syntax_id(value) : 0;
    row->caller = callable_capability_current_declaration(ctx); row->caller_id = ast_node_stable_id(row->caller);
    semantic_param_publication_projection_record_identifier(ctx, value);
    row->meta |= meta; return semantic_indexed_string_borrow_publish_source(value, ctx);
}
bool semantic_param_publication_record_string_concat(SemanticContext *ctx, ASTNode *value)
{
    if (!semantic_param_publication_pending(ctx) || value == NULL || value->type != AST_BINARY ||
        ast_binary_operator(value).type != TOKEN_PLUS) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, true);
    if (row == NULL || row->arguments != NULL) return false;
    PublishedCallArgument operands[2];
    ASTNode *nodes[2] = {ast_binary_left(value), ast_binary_right(value)};
    for (size_t i = 0; i < 2; i++) {
        PublishedNode *operand = publication_node(ctx->function_param_publications, nodes[i], false);
        const Type *type = operand == NULL ? NULL : operand->value_type;
        const Type *effective = type_is_slot_handle(type) ? type_slot_inner_type(type) : type;
        if (operand == NULL || !operand->value_present || operand->value_shape != nodes[i]->type ||
            operand->id == 0 || effective != TYPE_STRING || effective->kind != TYPE_KIND_PRIMITIVE ||
            (nodes[i]->type == AST_IDENTIFIER && operand->binding_id != ast_identifier_binding_syntax_id(nodes[i])) ||
            (nodes[i]->type == AST_CALL && !publication_call_ready(operand))) return false;
        operands[i] = (PublishedCallArgument){nodes[i], operand->id, nodes[i]->type,
            ast_identifier_binding_syntax_id(nodes[i]), type, 0, 0};
    }
    row->arguments = malloc(sizeof(operands));
    if (row->arguments == NULL) return false;
    memcpy(row->arguments, operands, sizeof(operands));
    row->argument_count = 2;
    row->builtin_string_concat = true;
    return true;
}
bool semantic_param_publication_record_execution(SemanticContext *ctx,
    const ASTNode *statement, bool reachable)
{
    if (!semantic_param_publication_pending(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, statement, true);
    if (row == NULL) return false;
    row->execution_present = true; row->reachable |= reachable; return true;
}
bool semantic_param_publication_record_checked_call(SemanticContext *ctx, ASTNode *call)
{
    if (!semantic_param_publication_pending(ctx) || call == NULL || call->type != AST_CALL) return false;
    const ASTNode *target = ast_call_callee(call);
    uint32_t builtin = 0;
    bool kind_present = ast_call_semantic_callee_builtin_kind(call, &builtin);
    if (target == NULL || ast_node_stable_id(target) == 0) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, true);
    if (row == NULL) return false;
    size_t count = ast_call_arg_count(call);
    if (!row->call_present) {
        if (count > SIZE_MAX / sizeof(*row->arguments)) return false;
        free(row->arguments); free(row->target_name);
        row->arguments = count == 0 ? NULL : calloc(count, sizeof(*row->arguments));
        row->target_name = NULL;
        if (count != 0 && row->arguments == NULL) return false;
        const char *name = target->type == AST_IDENTIFIER ? ast_identifier_name(target) :
            target->type == AST_MEMBER_ACCESS ? ast_member_name(target) : NULL;
        row->target_name = pergyra_strdup(name);
        if (name != NULL && row->target_name == NULL) return false;
    } else if (row->argument_count != count || row->target != target) return false;
    row->target = target; row->target_id = ast_node_stable_id(target); row->builtin = builtin;
    row->target_shape = target->type; row->target_binding = ast_identifier_binding_syntax_id(target);
    row->kind_present = kind_present;
    row->caller = callable_capability_current_declaration(ctx);
    row->caller_id = ast_node_stable_id(row->caller);
    if (row->caller != NULL && row->caller_id == 0) return false;
    row->execution_present = true;
    row->reachable |= ctx->future_lifecycle_unreachable_depth == 0;
    row->declaration = ast_call_semantic_callee_decl_id(call);
    row->value_binding = ast_call_semantic_callee_value_binding_id(call);
    row->stdlib = ast_call_semantic_callee_is_stdlib(call); row->declared_callable = ast_call_semantic_callee_declared_callable(call);
    row->call_receiver = target->type == AST_MEMBER_ACCESS ? ast_member_object(target) : NULL;
    row->call_receiver_id = ast_node_stable_id(row->call_receiver);
    PublishedNode *receiver = publication_node(ctx->function_param_publications, row->call_receiver, false);
    const Type *receiver_type = receiver != NULL && receiver->value_present && !receiver->meta
        ? receiver->value_type : NULL;
    size_t formal_count = 0;
    if (row->selected_callable != NULL &&
        (!row->declared_callable || row->selected_callable_id != row->declaration ||
         !semantic_host_index_selected_call_formal(ctx, call, row->selected_callable,
             receiver_type, row->caller, count, &formal_count))) return false;
    if (row->selected_callable != NULL &&
        formal_count != ast_func_param_count(row->selected_callable)) return false;
    if (target->type == AST_IDENTIFIER && row->value_present && row->value_type != NULL) {
        Symbol *symbol = lookup_identifier_symbol((ASTNode *)target, ctx);
        ASTNode *decl = symbol != NULL && symbol->kind == SYMBOL_CLASS
            ? semantic_constructor_decl_for_symbol_kind(ctx, symbol->kind, symbol->name) : NULL;
        if (decl != NULL && decl->type == AST_CLASS_DECL && !ast_class_is_struct(decl) &&
            symbol->decl_syntax_id != 0 && symbol->decl_syntax_id == ast_node_stable_id(decl) &&
            symbol->type == row->value_type && row->value_type->kind == TYPE_KIND_CLASS &&
            row->value_type->nominal_flavor == TYPE_NOMINAL_CLASS &&
            row->value_binding == 0 && (row->declaration == 0 || row->declaration == symbol->decl_syntax_id)) {
            row->unmodelled_constructor = decl;
            row->unmodelled_constructor_id = symbol->decl_syntax_id;
            row->unmodelled_constructor_type = row->value_type;
        }
    }
    row->local = kind_present && row->declaration == 0 && row->value_binding == 0 &&
        (builtin != BUILTIN_NOT_BUILTIN || row->stdlib) && builtin != BUILTIN_WRITE && row->target_name != NULL &&
        slot_builtin_call_is_local_non_escape(row->target_name);
    for (size_t i = 0; i < count; i++) {
        const ASTNode *arg = ast_call_argument(call, i);
        if (arg == NULL || ast_node_stable_id(arg) == 0) return false;
        PublishedNode *value = publication_node(ctx->function_param_publications, arg, false);
        size_t formal = i;
        if (row->selected_callable != NULL &&
            !semantic_host_index_selected_call_formal(ctx, call, row->selected_callable,
                receiver_type, row->caller, i, &formal)) return false;
        row->arguments[i] = (PublishedCallArgument){arg, ast_node_stable_id(arg), arg->type,
            ast_identifier_binding_syntax_id(arg), value != NULL && value->value_present ? value->value_type : NULL,
            formal, row->selected_callable == NULL ? 0 :
                ast_func_param_stable_id(ast_func_param(row->selected_callable, formal))};
    }
    row->argument_count = count; row->call_present = true;
    bool checked_finish = publication_checked_intrinsic(ctx, call,
        BUILTIN_TEXT_BUILDER_FINISH, TYPE_STRING, 2) != NULL &&
        row->arguments[0].type == TYPE_TEXT_BUILDER && row->arguments[1].type == TYPE_ALLOCATOR;
    bool checked_destroy = publication_checked_intrinsic(ctx, call,
        BUILTIN_ALLOCATOR_DESTROY, TYPE_VOID, 1) != NULL && row->arguments[0].type == TYPE_ALLOCATOR;
    if (!checked_finish && !checked_destroy)
        for (size_t p = 0; p < count; p++)
            if (row->arguments[p].type == TYPE_ALLOCATOR)
                semantic_param_publication_invalidate_allocator_binding(ctx, row->arguments[p].node);
    if (row->kind_present && row->builtin == BUILTIN_READ && row->value_present && !row->physical_scalar &&
        !publication_record_read_payload_loan(ctx, call)) return false;
    if (row->value_present && row->string_value && semantic_indexed_string_independent_result(call, ctx))
        row->origin_ready = row->copy_escape = true;
    if (row->private_array_push || row->private_physical_mutation) {
        function_param_flow_claims_destroy(row->deferred_syntax);
        row->deferred_syntax = NULL;
    }
    row->private_array_push = false;
    row->private_physical_mutation = false;
    if (!publication_record_private_array_push(ctx, call, row)) return false;
    row = publication_node(ctx->function_param_publications, call, false);
    if (row == NULL || !publication_record_private_physical_array_mutation(ctx, call, row)) return false;
    row = publication_node(ctx->function_param_publications, call, false);
    return row != NULL && (!row->string_value || semantic_indexed_string_borrow_publish_source(call, ctx));
}
bool publication_call_ready(const PublishedNode *row)
{
    uint32_t builtin = 0;
    if (row == NULL || !row->call_present || row->node->type != AST_CALL || row->target == NULL ||
        row->id != ast_node_stable_id(row->node) || row->target != ast_call_callee(row->node) ||
        row->caller_id != ast_node_stable_id(row->caller) ||
        row->target_id != ast_node_stable_id(row->target) ||
        row->target_shape != row->target->type || row->target_binding != ast_identifier_binding_syntax_id(row->target) ||
        row->call_receiver != (row->target_shape == AST_MEMBER_ACCESS ? ast_member_object(row->target) : NULL) ||
        row->call_receiver_id != ast_node_stable_id(row->call_receiver) ||
        (row->target_shape == AST_MEMBER_ACCESS && row->call_receiver_id == 0) ||
        ast_call_semantic_callee_builtin_kind(row->node, &builtin) != row->kind_present ||
        (row->kind_present && builtin != row->builtin) ||
        row->declaration != ast_call_semantic_callee_decl_id(row->node) ||
        row->value_binding != ast_call_semantic_callee_value_binding_id(row->node) ||
        row->stdlib != ast_call_semantic_callee_is_stdlib(row->node) || row->declared_callable != ast_call_semantic_callee_declared_callable(row->node) ||
        row->argument_count != ast_call_arg_count(row->node) ||
        (row->selected_callable != NULL && (row->selected_callable->type != AST_FUNC_DECL ||
         row->selected_callable_id == 0 || row->selected_callable_id != row->declaration ||
         row->selected_callable_id != ast_node_stable_id(row->selected_callable))) ||
        (row->argument_count != 0 && row->arguments == NULL)) return false;
    for (size_t i = 0; i < row->argument_count; i++) {
        const PublishedCallArgument *arg = &row->arguments[i];
        if (arg->node != ast_call_argument(row->node, i) || arg->node == NULL || arg->shape != arg->node->type ||
            arg->id != ast_node_stable_id(arg->node) || arg->binding != ast_identifier_binding_syntax_id(arg->node) ||
            (row->selected_callable != NULL && (arg->formal_ordinal >= ast_func_param_count(row->selected_callable) ||
             arg->formal_id == 0 || arg->formal_id != ast_func_param_stable_id(
                 ast_func_param(row->selected_callable, arg->formal_ordinal))))) return false;
    }
    const char *name = row->target->type == AST_IDENTIFIER ? ast_identifier_name(row->target) :
        row->target->type == AST_MEMBER_ACCESS ? ast_member_name(row->target) : NULL;
    return row->target_name == NULL || (name != NULL && strcmp(row->target_name, name) == 0);
}
typedef struct PublicationConcatVisit {
    const ASTNode *node;
    const struct PublicationConcatVisit *parent;
} PublicationConcatVisit;
static PublishedNode *publication_current_value_at(const SemanticContext *ctx,
    const ASTNode *value, const PublicationConcatVisit *parent)
{
    if (!semantic_param_publication_inputs_ready(ctx)) return NULL;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, false);
    if (row == NULL || !row->value_present || row->value_shape != value->type ||
        (value->type == AST_CALL && !semantic_param_publication_checked_call(ctx, value)) || (value->type == AST_IDENTIFIER &&
        row->binding_id != ast_identifier_binding_syntax_id(value))) return NULL;
    if (row->kind_present && row->builtin == BUILTIN_TEXT_BUILDER_FINISH &&
        (row->origin_ready || row->copy_escape) &&
        !semantic_param_publication_checked_finish_independent(ctx, value)) return NULL;
    if (row->empty_map_declaration != NULL && (row->empty_map_declaration_id == 0 ||
        row->empty_map_declaration_id != ast_node_stable_id(row->empty_map_declaration) ||
        !semantic_param_publication_empty_map_binding(ctx, row->empty_map_declaration))) return NULL;
    if (row->owned_slot_declaration != NULL) {
        SemanticPublishedSlotBirth birth;
        if (row->owned_slot_declaration_id == 0 || row->owned_slot_declaration_id != ast_node_stable_id(row->owned_slot_declaration) ||
            !semantic_param_publication_owned_slot_binding(ctx, row->owned_slot_declaration, &birth)) return NULL;
    }
    if (value->type == AST_BINARY && row->string_value && row->origin_ready &&
        !row->builtin_string_concat) return NULL;
    if (row->builtin_string_concat) {
        for (const PublicationConcatVisit *visit = parent; visit != NULL; visit = visit->parent)
            if (visit->node == value) return NULL;
        if (value->type != AST_BINARY || ast_binary_operator(value).type != TOKEN_PLUS ||
            !row->string_value || row->value_type != TYPE_STRING || row->value_type->kind != TYPE_KIND_PRIMITIVE ||
            !row->origin_ready || row->argument_count != 2 || row->arguments == NULL) return NULL;
        PublicationConcatVisit visit = {value, parent};
        const ASTNode *nodes[2] = {ast_binary_left(value), ast_binary_right(value)};
        for (size_t i = 0; i < 2; i++) {
            const PublishedCallArgument *snapshot = &row->arguments[i];
            if (nodes[i] == NULL || snapshot->node != nodes[i] || snapshot->id == 0 ||
                snapshot->id != ast_node_stable_id(nodes[i]) || snapshot->shape != nodes[i]->type ||
                snapshot->binding != ast_identifier_binding_syntax_id(nodes[i])) return NULL;
            PublishedNode *operand = publication_current_value_at(ctx, nodes[i], &visit);
            const Type *type = operand == NULL ? NULL : operand->value_type;
            const Type *effective = type_is_slot_handle(type) ? type_slot_inner_type(type) : type;
            if (operand == NULL || snapshot->type != type || effective != TYPE_STRING ||
                effective->kind != TYPE_KIND_PRIMITIVE) return NULL;
        }
    }
    return row;
}
PublishedNode *publication_current_value(const SemanticContext *ctx, const ASTNode *value)
{ return publication_current_value_at(ctx, value, NULL); }
bool semantic_param_publication_checked_local_owner_constructor(
    const SemanticContext *ctx, const ASTNode *value)
{
    if (value != NULL && value->type == AST_BINARY) {
        PublishedNode *concat = publication_current_value(ctx, value);
        return concat != NULL && !concat->meta && concat->value_type == TYPE_STRING &&
            concat->builtin_string_concat && concat->origin_ready &&
            concat->argument_count == 2 && ast_binary_operator(value).type == TOKEN_PLUS;
    }
    PublishedNode *row = publication_result_current(ctx, value);
    if (row == NULL || row->meta || row->declared_callable || row->declaration != 0 || row->value_binding != 0 ||
        !row->kind_present || row->nominal_result != NULL || row->result_relation ||
        row->target == NULL || row->target->type != AST_IDENTIFIER || row->target_binding != 0 ||
        row->target_id == 0 || row->target_name == NULL || row->id == 0) return false;
    if (semantic_array_storage_independent_physical_hof_result(value, (SemanticContext *)ctx))
        return true;
    /* These constructor contracts prove no input retention and independent
     * result backing. They confer no copy, cleanup or Finish lifetime right. */
    if (row->empty_map_declaration != NULL) {
        const ASTNode *declaration = row->empty_map_declaration;
        return row->stdlib && row->builtin == BUILTIN_NOT_BUILTIN &&
            declaration->type == AST_LET_DECL &&
            row->empty_map_declaration_id != 0 &&
            row->empty_map_declaration_id == ast_node_stable_id(declaration) &&
            ast_let_initializer(declaration) == value &&
            semantic_param_publication_empty_map_binding(ctx, declaration);
    }
    if (row->builtin == BUILTIN_CONCAT)
        return row->stdlib && row->value_type == TYPE_STRING && row->argument_count == 2 &&
            strcmp(row->target_name, "Concat") == 0 && row->arguments[0].type == TYPE_STRING &&
            row->arguments[1].type == TYPE_STRING &&
            semantic_param_publication_argument_borrowed_for_call(ctx, value, 0) &&
            semantic_param_publication_argument_borrowed_for_call(ctx, value, 1);
    /* The current typed publication owns the result type; a present AST type
       projection must agree, but stdlib calls need not carry that projection.
       ToString(String) is identity; Bool may be static text. This proves input-
       backing independence, never fresh allocation or cleanup authority. */
    if (row->stdlib && row->builtin == BUILTIN_NOT_BUILTIN && row->value_type == TYPE_STRING &&
        (ast_call_semantic_value_type_name(value) == NULL ||
         strcmp(ast_call_semantic_value_type_name(value), "String") == 0)) {
        bool substring = strcmp(row->target_name, "Substring") == 0;
        bool scalar = strcmp(row->target_name, "ToString") == 0;
        if ((substring || scalar || strcmp(row->target_name, "StringReplace") == 0) &&
            row->argument_count == (scalar ? 1u : 3u)) {
            for (size_t p = 0; p < row->argument_count; p++) {
                const PublishedCallArgument *arg = &row->arguments[p];
                if (scalar ? !publication_physical_scalar(arg->type) :
                    arg->type != (substring && p != 0 ? TYPE_INT : TYPE_STRING) ||
                    ((p == 0 || !substring) &&
                     !semantic_param_publication_argument_borrowed_for_call(ctx, value, p))) return false;
                if (arg->shape == AST_CALL && ast_call_semantic_value_type_name(arg->node) != NULL && (
                    strcmp(ast_call_semantic_value_type_name(arg->node), arg->type->name) != 0)) return false;
            }
            return true;
        }
    }
    if (row->stdlib) return false;
    if (row->builtin == BUILTIN_ALLOCATOR_RESULT)
        return row->value_type == TYPE_ALLOCATOR && row->argument_count == 0;
    if (row->builtin != BUILTIN_TEXT_BUILDER_NEW || row->value_type != TYPE_TEXT_BUILDER ||
        row->argument_count != 1 || row->arguments[0].id == 0 ||
        !semantic_param_publication_checked_argument(ctx, value, 0)) return false;
    PublishedNode *capacity = publication_current_value(ctx, row->arguments[0].node);
    return capacity != NULL && !capacity->meta &&
        (capacity->value_type == TYPE_INT || capacity->value_type == TYPE_LONG) &&
        row->arguments[0].type == capacity->value_type;
}
bool semantic_param_publication_record_nominal_result(SemanticContext *ctx,
    ASTNode *call, SemanticNominalResultSnapshot *snapshot)
{
    if (snapshot == NULL || (snapshot->field_count != 0 && snapshot->fields == NULL) || snapshot->result_type_name == NULL ||
        snapshot->field_count != ast_call_arg_count(call) ||
        !semantic_param_publication_record_checked_call(ctx, call)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    if (row == NULL || row->nominal_result != NULL) return false;
    for (size_t i = 0; i < row->argument_count; i++) {
        PublishedNode *arg = publication_node(ctx->function_param_publications, row->arguments[i].node, false);
        if (arg == NULL || !arg->value_present ||
            arg->value_shape != arg->node->type || snapshot->fields[i].argument_type_name == NULL ||
            (arg->node->type == AST_IDENTIFIER && arg->binding_id != ast_identifier_binding_syntax_id(arg->node)) ||
            (arg->node->type == AST_CALL && !publication_call_ready(arg))) return false;
        char *name = pergyra_strdup(snapshot->fields[i].argument_type_name);
        if (name == NULL) return false;
        free(arg->value_type_name); arg->value_type_name = name;
    }
    row->nominal_result = snapshot;
    return true;
}
PublishedNode *publication_result_current(const SemanticContext *ctx, const ASTNode *call)
{
    if (call == NULL || call->type != AST_CALL) return NULL;
    PublishedNode *row = publication_current_value(ctx, call);
    if (row == NULL || row->value_type == NULL || row->value_type == TYPE_UNKNOWN ||
        (row->nominal_result != NULL && (row->value_type_name == NULL ||
         row->value_type->kind != TYPE_KIND_CLASS || row->value_type->nominal_flavor != row->nominal_result->flavor ||
         !constructor_payload_type_ready(row->value_type, true) ||
         (row->nominal_result->flavor == TYPE_NOMINAL_SUBJECT &&
          semantic_host_decl_for_type((SemanticContext *)ctx, row->value_type) != row->nominal_result->decl) ||
         !semantic_nominal_result_snapshot_current(ctx, call, row->nominal_result) ||
         row->value_type->name == NULL || strcmp(row->value_type->name, row->value_type_name) != 0 ||
         strcmp(row->value_type_name, row->nominal_result->result_type_name) != 0))) return NULL;
    for (size_t i = 0; i < row->argument_count; i++) {
        PublishedNode *value = publication_current_value(ctx, row->arguments[i].node);
        if (value == NULL || row->arguments[i].type == NULL || row->arguments[i].type == TYPE_UNKNOWN ||
            row->arguments[i].type != value->value_type ||
            (row->nominal_result != NULL && (value->value_type_name == NULL ||
             strcmp(value->value_type_name, row->nominal_result->fields[i].argument_type_name) != 0 ||
             (value->node->type == AST_CALL && ast_call_semantic_value_type_name(value->node) != NULL &&
              strcmp(value->value_type_name, ast_call_semantic_value_type_name(value->node)) != 0)))) return NULL;
    }
    return row;
}
bool semantic_param_publication_nominal_result_complete(const SemanticContext *ctx, const ASTNode *call)
{
    PublishedNode *row = publication_result_current(ctx, call);
    return row != NULL && row->nominal_result != NULL;
}
SemanticPublishedValue semantic_param_publication_value(const SemanticContext *ctx, const ASTNode *value)
{
    PublishedNode *row = publication_current_value(ctx, value);
    if (row == NULL) return (SemanticPublishedValue){0};
    return (SemanticPublishedValue){true, row->copy_escape, row->meta, row->category, row->origin_ready, row->value_type};
}
bool semantic_param_publication_execution(const SemanticContext *ctx,
    const ASTNode *statement, bool *reachable)
{
    if (reachable == NULL || !semantic_param_publication_inputs_ready(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, statement, false);
    if (row == NULL || !row->execution_present) return false;
    *reachable = row->reachable; return true;
}
bool semantic_param_publication_argument_borrowed_for_call(const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    if (!semantic_param_publication_checked_argument(ctx, call, ordinal)) return false;
    if (!semantic_param_publication_value(ctx, ast_call_argument(call, ordinal)).ok) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    PgyRegionRetentionKind retention = PGY_REGION_RETENTION_UNKNOWN;
    if (row->declaration != 0 || row->value_binding != 0 || !row->kind_present) return false;
    return (row->builtin != BUILTIN_NOT_BUILTIN ?
        semantic_region_retention_summary_for_builtin(row->builtin, ordinal, &retention) :
        semantic_region_retention_summary_for_stdlib(call, ordinal, &retention)) && retention == PGY_REGION_RETENTION_BORROWED_FOR_CALL;
}
bool semantic_param_publication_local_call(const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    if (!semantic_param_publication_checked_argument(ctx, call, ordinal)) return false;
    return publication_node(ctx->function_param_publications, call, false)->local ||
        semantic_param_publication_argument_borrowed_for_call(ctx, call, ordinal);
}
bool semantic_param_publication_string_argument_read_only(
    const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    if (!semantic_param_publication_argument_borrowed_for_call(ctx, call, ordinal) ||
        ast_call_callee(call) == NULL || ast_call_callee(call)->type != AST_IDENTIFIER) return false;
    PublishedNode *value = publication_current_value(ctx, ast_call_argument(call, ordinal));
    /* Registered String operands are const-input reads or copied inputs.
     * Mutation-capable receivers such as Write's Slot are not String values. */
    return value != NULL && value->string_value;
}
uint32_t semantic_param_publication_binding(const SemanticContext *ctx, const ASTNode *value)
{
    return value != NULL && value->type == AST_IDENTIFIER && semantic_param_publication_value(ctx, value).ok ?
        ast_identifier_binding_syntax_id(value) : 0;
}
bool semantic_param_publication_record_assignment_replacement(SemanticContext *ctx,
    ASTNode *assignment, uint32_t binding, const Type *target_type)
{
    if (!semantic_param_publication_pending(ctx) || assignment == NULL ||
        assignment->type != AST_ASSIGNMENT || binding == 0 || target_type == NULL ||
        target_type == TYPE_UNKNOWN || type_is_slot_handle(target_type) ||
        type_is_resource_handle(target_type)) return false;
    ASTNode *target = ast_assignment_target(assignment), *value = ast_assignment_value(assignment);
    const char *type_name = ast_assignment_semantic_binding_type_name(assignment);
    if (target == NULL || target->type != AST_IDENTIFIER || value == NULL ||
        ast_identifier_binding_syntax_id(target) != binding || ast_node_stable_id(target) == 0 ||
        ast_node_stable_id(value) == 0 || type_name == NULL || target_type->name == NULL ||
        strcmp(type_name, target_type->name) != 0) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, assignment, true);
    char *name = pergyra_strdup(type_name);
    if (row == NULL || name == NULL) { free(name); return false; }
    free(row->target_name); row->target_name = name;
    row->target = target; row->target_id = ast_node_stable_id(target);
    row->target_shape = target->type; row->target_binding = binding;
    row->assignment_rhs = value; row->assignment_rhs_id = ast_node_stable_id(value);
    row->assignment_rhs_shape = value->type;
    row->assignment_rhs_binding = ast_identifier_binding_syntax_id(value);
    row->assignment_replacement = true;
    return true;
}
bool semantic_param_publication_assignment_replacement(const SemanticContext *ctx, const ASTNode *assignment)
{
    PublishedNode *row = publication_current_value(ctx, assignment);
    if (row == NULL || !row->assignment_replacement || assignment->type != AST_ASSIGNMENT ||
        row->target == NULL || row->target != ast_assignment_target(assignment) ||
        row->target->type != AST_IDENTIFIER || row->target_shape != row->target->type ||
        row->target_id == 0 || row->target_id != ast_node_stable_id(row->target) ||
        row->target_binding == 0 || row->target_binding != semantic_param_publication_binding(ctx, row->target) ||
        row->assignment_rhs == NULL || row->assignment_rhs != ast_assignment_value(assignment) ||
        row->assignment_rhs_id == 0 || row->assignment_rhs_id != ast_node_stable_id(row->assignment_rhs) ||
        row->assignment_rhs_shape != row->assignment_rhs->type ||
        row->assignment_rhs_binding != ast_identifier_binding_syntax_id(row->assignment_rhs) ||
        !semantic_param_publication_value(ctx, row->assignment_rhs).ok || row->target_name == NULL ||
        ast_assignment_semantic_binding_type_name(assignment) == NULL ||
        strcmp(row->target_name, ast_assignment_semantic_binding_type_name(assignment)) != 0) return false;
    return true;
}
bool semantic_param_publication_value_call_target(const SemanticContext *ctx, const ASTNode *call)
{
    if (!semantic_param_publication_checked_call(ctx, call)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    if (row == NULL || row->value_binding != 0 || row->target == NULL ||
        row->target->type != AST_IDENTIFIER) return false;
    if (row->nominal_result != NULL) {
        bool depends = false, retains = true;
        return semantic_nominal_result_snapshot_current(ctx, call, row->nominal_result) &&
            (row->argument_count == 0 ||
             (semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && !retains));
    }
    return row->kind_present && row->declaration == 0 &&
        (row->builtin != BUILTIN_NOT_BUILTIN || row->stdlib);
}

/* Pending typed projection owns the child-to-formal identity; ready-only
 * projection queries cannot be used while the String argument is issued. */
bool semantic_param_publication_pending_projection_formal_root(SemanticContext *ctx,
    ASTNode *caller, const ASTNode *value, const ASTNode **root_out, size_t *formal_out)
{
    if (ctx == NULL || caller == NULL || value == NULL || root_out == NULL ||
        formal_out == NULL || !semantic_param_publication_pending(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *row = publication_node(store, value, false);
    const PublishedProjection *p = row == NULL ? NULL : &row->projection;
    if (p == NULL || !p->present || row->caller != caller ||
        row->caller_id != ast_node_stable_id(caller) || p->root == NULL ||
        p->root->type != AST_IDENTIFIER || p->root_id == 0 ||
        p->root_id != ast_node_stable_id(p->root) ||
        p->root_binding_id != ast_identifier_binding_syntax_id(p->root))
        return false;
    PublishedNode *root = publication_node(store, p->root, false);
    const Type *type = root == NULL ? NULL : root->value_type;
    if (root == NULL || !root->value_present || root->meta || type == NULL ||
        !(type_is_constructed_named(type, "Array") ||
          type_is_constructed_named(type, "Slice") ||
          type->kind == TYPE_KIND_CLASS || type->kind == TYPE_KIND_TUPLE))
        return false;
    size_t found = SIZE_MAX;
    for (size_t pidx = 0; pidx < ast_func_param_count(caller); pidx++)
        if (p->root_binding_id == ast_func_param_stable_id(ast_func_param(caller, pidx))) {
            if (found != SIZE_MAX) return false;
            found = pidx;
        }
    if (found == SIZE_MAX) return false;
    *root_out = p->root;
    *formal_out = found;
    return true;
}
