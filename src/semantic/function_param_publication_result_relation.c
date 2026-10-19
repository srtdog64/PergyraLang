/* Checked result dependence belongs to the typed publication receipt. */
#include "function_param_publication_internal.h"
#include "type_checker_internal.h"
#include "type_checker_call_constructor_internal.h"
#include "../parser/ast_api.h"
#include "../common/string_compat.h"
#include <stdlib.h>
#include <string.h>

/* Whether C and LLVM copy every part of a value of `type` into new storage
 * (2026-10-19_struct_clone_is_deep): a physical scalar; a String
 * (StringConcat with ""); an Array of physical scalars or Strings
 * (pgy_array_clone_<T>, every String copied); an Option of such a value; a
 * non-generic struct or class of such fields, and a subject of them as the
 * argument itself. The backends copy any other part by value, so it may
 * share storage with the argument. */
static bool clone_copies_deeply(SemanticContext *ctx, const Type *type, int depth)
{
    if (type == NULL || depth > 16) return false;
    if (type_equals(type, TYPE_STRING) || publication_physical_scalar(type)) return true;
    if (type_is_constructed_named(type, "Array")) {
        const Type *element = type_get_constructed_arg(type, 0);
        return type_equals(element, TYPE_STRING) || publication_physical_scalar(element);
    }
    if (type_is_constructed_named(type, "Option"))
        return clone_copies_deeply(ctx, type_get_constructed_arg(type, 0), depth + 1);
    /* A generic instance is a constructed type, not a class type. A subject
     * is copied field by field only as the Clone argument itself: as a
     * field it keeps its identity and is copied by value. */
    ASTNode *decl = type->kind == TYPE_KIND_CLASS ? semantic_host_decl_for_type(ctx, type) : NULL;
    NominalDeclKind kind = decl != NULL && decl->type == AST_CLASS_DECL ? ast_class_nominal_kind(decl) : NOMINAL_DECL_OBJECT;
    if (kind != NOMINAL_DECL_STRUCT && kind != NOMINAL_DECL_CLASS && !(kind == NOMINAL_DECL_SUBJECT && depth == 0))
        return false;
    for (size_t i = 0;; i++) {
        const char *name = NULL;
        Type *field = constructor_decl_field_resolved_type_at(decl, i, &name, ctx);
        if (name == NULL) return true;
        if (!clone_copies_deeply(ctx, field, depth + 1)) return false;
    }
}

static bool publication_result_relation(const SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, bool backing, bool *result_depends, bool *external_retains)
{
    if (result_depends == NULL || external_retains == NULL) return false;
    PublishedNode *row = publication_result_current(ctx, call);
    if (row == NULL || ordinal >= row->argument_count) return false;
    if (row->nominal_result != NULL) {
        *result_depends = true; *external_retains = false; return true;
    }
    if (row->declaration != 0 && row->value_type != NULL && row->value_type->kind == TYPE_KIND_ENUM) {
        /* A payload enum constructor stores its arguments in the value it
         * returns, as a struct constructor does, and keeps nothing else.
         * Only the exact variant of the result's own enum qualifies. */
        ASTNode *owner = semantic_host_decl_for_type((SemanticContext *)ctx, row->value_type);
        size_t count = 0;
        char **variants = owner != NULL && owner->type == AST_ENUM_DECL ? ast_enum_variants(owner, &count) : NULL;
        bool variant = false;
        for (size_t i = 0; variants != NULL && i < count; i++)
            if (variants[i] != NULL && row->target_name != NULL && strcmp(variants[i], row->target_name) == 0 &&
                ast_enum_variant_param_count(owner, i) == row->argument_count) variant = true;
        const char *result_name = ast_call_semantic_value_type_name(call);
        if (!variant || ast_node_stable_id(owner) != row->declaration || row->value_binding != 0 ||
            row->stdlib || row->argument_count == 0 || ordinal >= row->argument_count ||
            result_name == NULL || row->value_type->name == NULL ||
            strcmp(result_name, row->value_type->name) != 0) return false;
        *result_depends = true; *external_retains = false; return true;
    }
    if (row->builtin == BUILTIN_CLONE) {
        /* A Clone the backends copy deeply shares nothing with its
         * argument; any other may share its storage. Clone keeps nothing
         * itself. */
        PublishedNode *input = row->argument_count == 1 ?
            publication_current_value(ctx, row->arguments[0].node) : NULL;
        if (!row->stdlib || !row->kind_present || row->declaration != 0 || row->value_binding != 0 ||
            row->target_shape != AST_IDENTIFIER || row->target_binding != 0 ||
            row->target_name == NULL || strcmp(row->target_name, "Clone") != 0 ||
            input == NULL || input->node != ast_call_argument(call, 0) ||
            !type_equals(input->value_type, row->value_type)) return false;
        *result_depends = !clone_copies_deeply((SemanticContext *)ctx, input->value_type, 0);
        *external_retains = false;
        return true;
    }
    if (row->builtin == BUILTIN_NOT_BUILTIN && row->stdlib && row->target_name != NULL &&
        strcmp(row->target_name, "ToString") == 0) {
        /* ToString of a physical scalar makes a new String; of anything
         * else, String interpolation parts included, its result may be the
         * argument itself. It keeps nothing. */
        PublishedNode *input = publication_current_value(ctx, row->arguments[ordinal].node);
        if (!row->kind_present || row->declaration != 0 || row->value_binding != 0 ||
            row->target_shape != AST_IDENTIFIER || row->target_binding != 0 ||
            row->argument_count != 1 || input == NULL || input->node != ast_call_argument(call, ordinal) ||
            !type_equals(row->value_type, TYPE_STRING)) return false;
        *result_depends = !publication_physical_scalar(input->value_type);
        *external_retains = false;
        return true;
    }
    if (row->builtin == BUILTIN_NOT_BUILTIN && row->stdlib && row->target_name != NULL &&
        (strcmp(row->target_name, "Split") == 0 || strcmp(row->target_name, "StringSplit") == 0)) {
        /* Split copies every part into a new Array of new Strings (runtime
         * StringSplit) and keeps neither argument. */
        PublishedNode *input = publication_current_value(ctx, row->arguments[ordinal].node);
        if (!row->kind_present || row->declaration != 0 || row->value_binding != 0 ||
            row->target_shape != AST_IDENTIFIER || row->target_binding != 0 ||
            row->argument_count != 2 || input == NULL || input->node != ast_call_argument(call, ordinal) ||
            !type_equals(input->value_type, TYPE_STRING) ||
            !type_is_constructed_named(row->value_type, "Array")) return false;
        *result_depends = false;
        *external_retains = false;
        return true;
    }
    if (row->builtin == BUILTIN_ALLOCATOR_POOL) {
        /* A pool is new storage sized by its scalar capacity; it keeps and
         * depends on nothing. Text finished with it is the allocator lane
         * owner's to check. */
        PublishedNode *input = publication_current_value(ctx, row->arguments[ordinal].node);
        if (!row->kind_present || row->declaration != 0 || row->value_binding != 0 ||
            row->argument_count != 1 || input == NULL || input->node != ast_call_argument(call, ordinal) ||
            !publication_physical_scalar(input->value_type) || row->value_type != TYPE_ALLOCATOR ||
            ast_call_semantic_value_type_name(call) == NULL) return false;
        *result_depends = false;
        *external_retains = false;
        return true;
    }
    if (row->builtin == BUILTIN_TEXT_BUILDER_FINISH) {
        /* The finished String takes the builder's buffer, which holds copies
         * of what was appended, so it depends on no builder input. A result
         * lane allocator gives it heap storage of its own; any other
         * allocator, a pool among them, holds it, so the result depends on
         * that argument (2026-10-19_finished_text_borrows_its_allocator). */
        if (!row->kind_present || row->declaration != 0 || row->value_binding != 0 ||
            row->argument_count != 2 || row->arguments[0].type != TYPE_TEXT_BUILDER ||
            row->arguments[1].type != TYPE_ALLOCATOR || !type_equals(row->value_type, TYPE_STRING) ||
            ast_call_semantic_value_type_name(call) == NULL) return false;
        *result_depends = ordinal == 1 && !semantic_param_publication_checked_finish_independent(ctx, call);
        *external_retains = false;
        return true;
    }
    if (row->builtin == BUILTIN_DIR_WALK) {
        /* DirWalk copies every path it finds into a new Array of new Strings
         * (runtime pgy_dir_walk_push_file) and keeps neither its root nor
         * anything else. */
        PublishedNode *input = publication_current_value(ctx, row->arguments[ordinal].node);
        if (!row->kind_present || row->declaration != 0 || row->value_binding != 0 ||
            row->target_shape != AST_IDENTIFIER || row->target_binding != 0 ||
            row->target_name == NULL || strcmp(row->target_name, "DirWalk") != 0 ||
            row->argument_count != 1 || input == NULL || input->node != ast_call_argument(call, ordinal) ||
            !type_equals(input->value_type, TYPE_STRING) ||
            !type_is_constructed_named(row->value_type, "Array") ||
            !type_equals(type_get_constructed_arg(row->value_type, 0), TYPE_STRING)) return false;
        *result_depends = false;
        *external_retains = false;
        return true;
    }
    PublishedNode *arg = publication_current_value(ctx, ast_call_argument(call, 0));
    const char *result = ast_call_semantic_value_type_name(call);
    if (arg == NULL || !row->result_relation || !row->kind_present ||
        row->declaration != 0 || row->value_binding != 0 ||
        row->argument_count != row->relation_argument_count || row->relation_argument != arg->node || row->relation_argument_id != arg->id ||
        row->relation_argument_type == NULL || row->relation_result_type == NULL || arg->value_type_name == NULL ||
        row->value_type_name == NULL || result == NULL || strcmp(row->relation_argument_type, arg->value_type_name) != 0 ||
        strcmp(row->relation_result_type, row->value_type_name) != 0 || strcmp(result, row->relation_result_type) != 0 ||
        (arg->node->type == AST_CALL && (ast_call_semantic_value_type_name(arg->node) == NULL ||
         strcmp(arg->value_type_name, ast_call_semantic_value_type_name(arg->node)) != 0))) return false;
    if (row->builtin == BUILTIN_READ) {
        const Type *receiver = row->relation_input_type;
        if (row->stdlib || row->target == NULL || row->target->type != AST_IDENTIFIER ||
            ast_identifier_name(row->target) == NULL || strcmp(ast_identifier_name(row->target), "Read") != 0 ||
            receiver == NULL || arg->value_type != receiver || receiver->kind != TYPE_KIND_SLOT ||
            type_is_write_view(receiver) || type_is_move_token(receiver) ||
            row->relation_output_type != row->value_type ||
            type_slot_inner_type(receiver) != row->relation_output_type ||
            row->argument_count != (type_slot_is_secure(receiver) ? 2u : 1u)) return false;
    } else if (row->builtin == BUILTIN_VIEW_READ) {
        if (row->stdlib || row->target == NULL || row->target->type != AST_IDENTIFIER ||
            ast_identifier_name(row->target) == NULL || strcmp(ast_identifier_name(row->target), "ViewRead") != 0 ||
            row->argument_count != 1 || arg->value_type != row->relation_input_type ||
            row->relation_input_type == NULL || row->relation_input_type->kind != TYPE_KIND_SLOT ||
            type_slot_access_mode(row->relation_input_type) != SLOT_ACCESS_OWNED ||
            row->value_type != row->relation_output_type || !type_is_read_view(row->value_type) ||
            type_slot_inner_type(row->value_type) != type_slot_inner_type(row->relation_input_type)) return false;
    } else if (row->builtin != BUILTIN_NOT_BUILTIN || !row->stdlib) return false;
    *result_depends = ordinal == 0 && (row->builtin != BUILTIN_READ || backing);
    *external_retains = false;
    return true;
}
bool semantic_param_publication_checked_result_relation(const SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, bool *result_depends, bool *external_retains)
{
    return publication_result_relation(ctx, call, ordinal, false, result_depends, external_retains);
}
bool semantic_param_publication_checked_backing_relation(const SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, bool *result_depends, bool *external_retains)
{
    return publication_result_relation(ctx, call, ordinal, true, result_depends, external_retains);
}
bool semantic_param_publication_record_checked_result_relation(SemanticContext *ctx, ASTNode *call,
    const Type *argument_type, const Type *result_type, uint32_t selected_decl, size_t payload_ordinal)
{
    if (!semantic_param_publication_pending(ctx) || call == NULL || call->type != AST_CALL ||
        selected_decl != 0 || payload_ordinal != 0 || ast_call_arg_count(call) == 0 ||
        argument_type == NULL || result_type == NULL || argument_type == TYPE_UNKNOWN || result_type == TYPE_UNKNOWN ||
        argument_type->name == NULL || result_type->name == NULL) return false;
    const char *result_name = ast_call_semantic_value_type_name(call);
    if (result_name == NULL || strcmp(result_name, result_type->name) != 0) return false;
    ASTNode *argument = ast_call_argument(call, 0);
    PublishedNode *row = publication_node(ctx->function_param_publications, call, true);
    if (row == NULL) return false;
    PublishedNode *arg = publication_node(ctx->function_param_publications, argument, false);
    if (arg == NULL || !arg->value_present || arg->value_shape != argument->type) return false;
    const char *argument_name = ast_call_semantic_value_type_name(argument);
    if (argument->type == AST_CALL && (!publication_call_ready(arg) ||
        arg->value_type != argument_type ||
        (argument_name != NULL && strcmp(argument_name, argument_type->name) != 0))) return false;
    char *a = pergyra_strdup(argument_type->name), *r = pergyra_strdup(result_type->name);
    char *av = pergyra_strdup(argument_type->name), *rv = pergyra_strdup(result_type->name);
    if (a == NULL || r == NULL || av == NULL || rv == NULL) { free(a); free(r); free(av); free(rv); return false; }
    /* Seal the original checked child call before publishing this relation.
     * Currency checks require this receipt; they must never synthesize it. */
    if (argument->type == AST_CALL && argument_name == NULL &&
        !ast_call_set_semantic_value_type_name_copy(argument, argument_type->name)) { free(a); free(r); free(av); free(rv); return false; }
    free(row->relation_argument_type); free(row->relation_result_type); free(arg->value_type_name); free(row->value_type_name);
    row->relation_argument_type = a; row->relation_result_type = r; arg->value_type_name = av; row->value_type_name = rv;
    row->relation_argument = argument; row->relation_argument_id = ast_node_stable_id(argument);
    row->relation_argument_count = ast_call_arg_count(call);
    row->relation_input_type = argument_type; row->relation_output_type = result_type;
    row->result_relation = row->relation_argument_id != 0;
    return row->result_relation;
}
