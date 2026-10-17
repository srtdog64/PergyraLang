// Typed member resolution and publication; expression dispatch owns entry.
#include <stdlib.h>
#include <string.h>
#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "function_param_flow_summary_internal.h"
#include "callable_capability_inference.h"
#include "../common/string_compat.h"
#include "diag_codes.h"

/* This receipt identifies a typed projection path. It says nothing about
 * whether the root's storage is local, independent, or safe to retain. */
void
semantic_param_publication_projection_destroy(PublishedNode *row)
{
    if (row == NULL) return;
    free(row->projection.name);
    free(row->projection.index_name);
    free(row->projection.field_type_name);
    free(row->projection.declaration_name);
    free(row->projection.root_name);
    memset(&row->projection, 0, sizeof(row->projection));
}

static ASTNode *
projection_identifier_root(SemanticContext *ctx, ASTNode *value)
{
    FunctionParamPublicationStore *store = ctx != NULL ? ctx->function_param_publications : NULL;
    if (store == NULL) return NULL;
    for (size_t depth = 0; value != NULL && depth <= store->node_count; depth++) {
        if (value->type == AST_IDENTIFIER)
            return ast_identifier_binding_syntax_id(value) != 0 ? value : NULL;
        value = value->type == AST_MEMBER_ACCESS ? ast_member_object(value) :
            value->type == AST_ARRAY_ACCESS ? ast_array_access_array(value) : NULL;
    }
    return NULL;
}

void
semantic_param_publication_projection_record_identifier(SemanticContext *ctx,
    ASTNode *value)
{
    if (ctx == NULL || ctx->has_error || value == NULL || value->type != AST_IDENTIFIER ||
        !semantic_param_publication_pending(ctx)) return;
    const char *name = ast_identifier_name(value);
    uint32_t binding = ast_identifier_binding_syntax_id(value);
    PublishedNode *row = publication_node(ctx->function_param_publications, value, false);
    if (row == NULL || name == NULL || binding == 0 || !row->value_present) return;
    char *copy = pergyra_strdup(name);
    if (copy == NULL) return;
    semantic_param_publication_projection_destroy(row);
    row->projection = (PublishedProjection){.present = true, .root = value,
        .root_id = row->id, .root_binding_id = binding,
        .result_type = row->value_type, .name = copy};
}

void
semantic_param_publication_projection_record_array(SemanticContext *ctx,
    ASTNode *value, ASTNode *receiver, ASTNode *index, const Type *receiver_type,
    const Type *index_type, const Type *result_type)
{
    if (!semantic_param_publication_pending(ctx) || ctx->has_error ||
        value == NULL || receiver == NULL || index == NULL ||
        (index->type != AST_IDENTIFIER && index->type != AST_NUMBER &&
         index->type != AST_BINARY && index->type != AST_UNARY &&
         index->type != AST_ARRAY_ACCESS)) return;
    ASTNode *root = projection_identifier_root(ctx, receiver);
    if (root == NULL || ast_node_stable_id(index) == 0 ||
        !type_equals(index_type, TYPE_INT) || result_type == TYPE_UNKNOWN) return;
    PublishedProjection receipt = {.present = true, .receiver = receiver,
        .receiver_id = ast_node_stable_id(receiver), .index = index,
        .index_id = ast_node_stable_id(index), .root = root,
        .root_id = ast_node_stable_id(root),
        .root_binding_id = ast_identifier_binding_syntax_id(root),
        .receiver_type = receiver_type, .index_type = index_type,
        .index_shape = index->type,
        .result_type = result_type};
    if (receipt.receiver_id == 0 || receipt.root_id == 0) return;
    if (index->type == AST_IDENTIFIER) {
        const char *name = ast_identifier_name(index);
        if (name == NULL || ast_identifier_binding_syntax_id(index) == 0) return;
        receipt.index_name = pergyra_strdup(name);
        if (receipt.index_name == NULL) return;
    } else {
        receipt.index_number = ast_number_value(index);
        receipt.index_long = ast_number_exact_long_value(index);
        receipt.index_flags = (unsigned)(ast_number_is_long(index) |
            (ast_number_is_float(index) << 1) | (ast_number_is_duration(index) << 2));
    }
    PublishedNode *row = publication_node(ctx->function_param_publications, value, false);
    if (row == NULL) { free(receipt.index_name); return; }
    semantic_param_publication_projection_destroy(row);
    row->projection = receipt;
}

static void
projection_record_member(SemanticContext *ctx, ASTNode *value,
    ASTNode *receiver, const Type *receiver_type, ASTNode *declaration,
    size_t ordinal, size_t count, PgyDeclField field,
    const Type *field_type, const Type *result_type)
{
    if (!semantic_param_publication_pending(ctx) || ctx->has_error ||
        declaration == NULL || declaration->type != AST_CLASS_DECL ||
        field.name == NULL || field.type_ast == NULL || field.type_ast->type != AST_TYPE ||
        field.declaration_syntax_id == 0 || result_type == TYPE_UNKNOWN ||
        field_type == TYPE_UNKNOWN || !type_equals(field_type, result_type)) return;
    GenericParams *generics = ast_type_generic_args(field.type_ast);
    const char *type_name = ast_type_name(field.type_ast);
    if ((generics != NULL && generics->count != 0) || type_name == NULL) return;
    ASTNode *root = projection_identifier_root(ctx, receiver);
    if (root == NULL) return;
    PublishedProjection receipt = {.present = true, .member = value,
        .member_id = ast_node_stable_id(value), .receiver = receiver,
        .receiver_id = ast_node_stable_id(receiver), .declaration = declaration,
        .declaration_id = ast_node_stable_id(declaration),
        .field_id = field.declaration_syntax_id, .field_type_node = field.type_ast,
        .field_type_id = ast_node_stable_id(field.type_ast),
        .root = root, .root_id = ast_node_stable_id(root),
        .root_binding_id = ast_identifier_binding_syntax_id(root),
        .field_ordinal = ordinal, .field_count = count,
        .declaration_flavor = nominal_flavor_from_decl(declaration),
        .declaration_struct = ast_class_is_struct(declaration),
        .field_is_string = field_type == TYPE_STRING,
        .receiver_type = receiver_type, .field_type = field_type,
        .result_type = result_type, .access = field.access,
        .explicit_access = field.has_explicit_access,
        .mutable_field = field.is_mutable, .vessel_field = field.is_vessel_field,
        .default_field = field.has_default};
    if (receipt.member_id == 0 || receipt.receiver_id == 0 || receipt.declaration_id == 0 ||
        receipt.field_type_id == 0 || receipt.root_id == 0) return;
    receipt.name = pergyra_strdup(field.name);
    receipt.field_type_name = pergyra_strdup(type_name);
    if (receipt.field_is_string && ast_class_name(declaration) != NULL)
        receipt.declaration_name = pergyra_strdup(ast_class_name(declaration));
    if (receipt.field_is_string && ast_identifier_name(root) != NULL)
        receipt.root_name = pergyra_strdup(ast_identifier_name(root));
    if (receipt.name == NULL || receipt.field_type_name == NULL ||
        (receipt.field_is_string && (receipt.declaration_name == NULL ||
         receipt.root_name == NULL))) {
        free(receipt.name); free(receipt.field_type_name);
        free(receipt.declaration_name); free(receipt.root_name);
        return;
    }
    PublishedNode *row = publication_node(ctx->function_param_publications, value, false);
    if (row == NULL) {
        free(receipt.name); free(receipt.field_type_name);
        free(receipt.declaration_name); free(receipt.root_name);
        return;
    }
    receipt.caller = callable_capability_current_declaration(ctx);
    receipt.caller_id = ast_node_stable_id(receipt.caller);
    semantic_param_publication_projection_destroy(row);
    row->projection = receipt;
}

bool
semantic_param_publication_projection_root_current(const SemanticContext *ctx,
    const ASTNode *value, ASTNode **caller_out, ASTNode **root_out)
{
    if (caller_out != NULL) *caller_out = NULL;
    if (root_out != NULL) *root_out = NULL;
    if (ctx == NULL || ctx->has_error || value == NULL ||
        !semantic_param_publication_inputs_ready(ctx)) return false;
    const FunctionParamPublicationStore *store = ctx->function_param_publications;
    const ASTNode *cursor = value, *caller = NULL, *root = NULL;
    for (size_t depth = 0; depth <= store->node_count; depth++) {
        PublishedNode *row = publication_current_value(ctx, cursor);
        if (row == NULL || row->meta || !row->projection.present ||
            row->projection.result_type != row->value_type ||
            row->caller == NULL || row->caller->type != AST_FUNC_DECL ||
            row->caller_id == 0 || row->caller_id != ast_node_stable_id(row->caller)) return false;
        if (caller == NULL) caller = row->caller;
        if (caller != row->caller) return false;
        const PublishedProjection *p = &row->projection;
        if (p->root == NULL || p->root_id == 0 ||
            p->root_id != ast_node_stable_id(p->root) ||
            p->root_binding_id == 0 ||
            p->root_binding_id != ast_identifier_binding_syntax_id(p->root)) return false;
        if (root == NULL) root = p->root;
        if (root != p->root) return false;
        if (cursor->type == AST_IDENTIFIER) {
            const char *name = ast_identifier_name(cursor);
            if (cursor != root || p->root_id != row->id ||
                p->root_binding_id != row->binding_id ||
                name == NULL || p->name == NULL || strcmp(name, p->name) != 0 ||
                !publication_routine_header_current(ctx, caller)) return false;
            if (caller_out != NULL) *caller_out = (ASTNode *)caller;
            if (root_out != NULL) *root_out = (ASTNode *)root;
            return true;
        }
        if (cursor->type != AST_MEMBER_ACCESS && cursor->type != AST_ARRAY_ACCESS)
            return false;
        const ASTNode *receiver = cursor->type == AST_MEMBER_ACCESS ?
            ast_member_object(cursor) : ast_array_access_array(cursor);
        PublishedNode *receiver_row = publication_current_value(ctx, receiver);
        if (receiver == NULL || receiver != p->receiver ||
            p->receiver_id == 0 || p->receiver_id != ast_node_stable_id(receiver) ||
            receiver_row == NULL || receiver_row->value_type != p->receiver_type)
            return false;
        if (cursor->type == AST_ARRAY_ACCESS) {
            const ASTNode *index = ast_array_access_index(cursor);
            PublishedNode *index_row = publication_current_value(ctx, index);
            if (index == NULL || index != p->index || index->type != p->index_shape ||
                p->index_id == 0 ||
                p->index_id != ast_node_stable_id(index) || index_row == NULL ||
                index_row->caller != caller ||
                index_row->caller_id != ast_node_stable_id(caller) ||
                index_row->value_type != p->index_type ||
                !type_equals(p->index_type, TYPE_INT) ||
                !(type_is_constructed_named(p->receiver_type, "Array") ||
                  type_is_constructed_named(p->receiver_type, "Slice")) ||
                !type_equals(type_get_constructed_arg(p->receiver_type, 0),
                    p->result_type)) return false;
            if (index->type == AST_IDENTIFIER) {
                const char *name = ast_identifier_name(index);
                if (p->index_name == NULL || name == NULL ||
                    strcmp(name, p->index_name) != 0 ||
                    ast_identifier_binding_syntax_id(index) == 0) return false;
            } else if (index->type == AST_NUMBER) {
                unsigned flags = (unsigned)(ast_number_is_long(index) |
                    (ast_number_is_float(index) << 1) |
                    (ast_number_is_duration(index) << 2));
                if (p->index_name != NULL || p->index_number != ast_number_value(index) ||
                    p->index_long != ast_number_exact_long_value(index) ||
                    p->index_flags != flags) return false;
            } else if ((index->type != AST_BINARY && index->type != AST_UNARY &&
                        index->type != AST_ARRAY_ACCESS) || p->index_name != NULL ||
                       !function_param_flow_typed_expression_children_current(
                           (SemanticContext *)ctx, (ASTNode *)caller, index)) return false;
        } else {
            const char *name = ast_member_name(cursor);
            if (p->index != NULL || !type_equals(p->field_type, p->result_type) ||
                p->declaration == NULL ||
                p->declaration->type != AST_CLASS_DECL ||
                p->declaration_id == 0 ||
                p->declaration_id != ast_node_stable_id(p->declaration) ||
                semantic_host_decl_for_type((SemanticContext *)ctx,
                    p->receiver_type) != p->declaration ||
                p->field_count != projection_source_field_count((ASTNode *)p->declaration) ||
                p->field_ordinal >= p->field_count || name == NULL ||
                p->name == NULL || strcmp(name, p->name) != 0 ||
                p->field_type_name == NULL || p->field_type_node == NULL ||
                p->field_type_node->type != AST_TYPE ||
                ast_type_name(p->field_type_node) == NULL ||
                strcmp(ast_type_name(p->field_type_node), p->field_type_name) != 0)
                return false;
            GenericParams *generics = ast_type_generic_args(p->field_type_node);
            if (generics != NULL && generics->count != 0) return false;
            PgyDeclField field = projection_source_field_at(
                (ASTNode *)p->declaration, p->field_ordinal);
            if (field.name == NULL || strcmp(field.name, p->name) != 0 ||
                field.declaration_syntax_id == 0 ||
                field.declaration_syntax_id != p->field_id ||
                field.type_ast != p->field_type_node ||
                p->field_type_id == 0 ||
                p->field_type_id != ast_node_stable_id(field.type_ast) ||
                field.access != p->access ||
                field.has_explicit_access != p->explicit_access ||
                field.is_mutable != p->mutable_field ||
                field.is_vessel_field != p->vessel_field ||
                field.has_default != p->default_field ||
                semantic_type_resolution_lookup_metadata_type_ref(
                    (SemanticContext *)ctx, field.type_ast) != p->field_type)
                return false;
        }
        cursor = receiver;
    }
    return false;
}

bool semantic_param_publication_member_projection_syntax_current(const ASTNode *value,
    const ASTNode *caller, const PublishedProjection *p)
{
    if (value == NULL || value->type != AST_MEMBER_ACCESS ||
        caller == NULL || caller->type != AST_FUNC_DECL || p == NULL ||
        !p->present || p->member != value || p->member_id == 0 ||
        p->member_id != ast_node_stable_id(value) || p->caller != caller ||
        p->caller_id == 0 || p->caller_id != ast_node_stable_id(caller) ||
        p->index != NULL || p->root == NULL || p->root->type != AST_IDENTIFIER ||
        p->receiver != p->root || p->receiver != ast_member_object(value) ||
        p->receiver_id == 0 || p->receiver_id != ast_node_stable_id(p->receiver) ||
        p->root_id == 0 || p->root_id != ast_node_stable_id(p->root) ||
        p->root_binding_id == 0 ||
        p->root_binding_id != ast_identifier_binding_syntax_id(p->root) ||
        p->root_name == NULL || ast_identifier_name(p->root) == NULL ||
        strcmp(p->root_name, ast_identifier_name(p->root)) != 0 ||
        p->declaration == NULL || p->declaration->type != AST_CLASS_DECL ||
        p->declaration_id == 0 ||
        p->declaration_id != ast_node_stable_id(p->declaration) ||
        p->declaration_flavor != nominal_flavor_from_decl(p->declaration) ||
        p->declaration_struct != ast_class_is_struct(p->declaration) ||
        p->declaration_name == NULL || ast_class_name(p->declaration) == NULL ||
        strcmp(p->declaration_name, ast_class_name(p->declaration)) != 0 ||
        ast_generic_param_count(ast_class_generic_params(p->declaration)) != 0 ||
        ast_class_field_destructure_count(p->declaration) != 0 ||
        p->field_count != projection_source_field_count((ASTNode *)p->declaration) ||
        p->field_ordinal >= p->field_count || !p->field_is_string ||
        p->field_type != TYPE_STRING || p->result_type != TYPE_STRING ||
        p->name == NULL || ast_member_name(value) == NULL ||
        strcmp(p->name, ast_member_name(value)) != 0 ||
        p->field_type_node == NULL || p->field_type_node->type != AST_TYPE ||
        p->field_type_id == 0 ||
        p->field_type_id != ast_node_stable_id(p->field_type_node) ||
        p->field_type_name == NULL || ast_type_name(p->field_type_node) == NULL ||
        strcmp(p->field_type_name, ast_type_name(p->field_type_node)) != 0)
        return false;
    GenericParams *generics = ast_type_generic_args(p->field_type_node);
    if (generics != NULL && generics->count != 0) return false;
    PgyDeclField field = projection_source_field_at(
        (ASTNode *)p->declaration, p->field_ordinal);
    return field.name != NULL && strcmp(field.name, p->name) == 0 &&
        field.declaration_syntax_id != 0 &&
        field.declaration_syntax_id == p->field_id &&
        field.type_ast == p->field_type_node &&
        field.access == p->access &&
        field.has_explicit_access == p->explicit_access &&
        field.is_mutable == p->mutable_field &&
        field.is_vessel_field == p->vessel_field &&
        field.has_default == p->default_field;
}

bool semantic_param_publication_selected_member_field_current(const SemanticContext *ctx,
    const ASTNode *value, ASTNode **caller_out, ASTNode **root_out,
    SemanticPublishedSelectedField *field_out)
{
    if (caller_out != NULL) *caller_out = NULL;
    if (root_out != NULL) *root_out = NULL;
    if (field_out != NULL) memset(field_out, 0, sizeof(*field_out));
    if (field_out == NULL || value == NULL || value->type != AST_MEMBER_ACCESS)
        return false;
    ASTNode *caller = NULL, *root = NULL;
    if (!semantic_param_publication_projection_root_current(ctx, value,
            &caller, &root)) return false;
    const ASTNode *receiver = ast_member_object(value);
    if (receiver == NULL || receiver->type != AST_IDENTIFIER || receiver != root)
        return false;
    PublishedNode *row = publication_current_value(ctx, value);
    if (row == NULL || row->id == 0 || row->id != ast_node_stable_id(value) ||
        !row->projection.present) return false;
    const PublishedProjection *p = &row->projection;
    if (p->receiver != receiver || p->root != root || p->declaration == NULL ||
        p->declaration->type != AST_CLASS_DECL || p->field_type_node == NULL ||
        p->field_type == NULL || p->field_type == TYPE_UNKNOWN ||
        p->receiver_type == NULL || p->receiver_type == TYPE_UNKNOWN ||
        p->field_id == 0 || p->field_ordinal >= p->field_count) return false;
    if (p->field_type == TYPE_STRING &&
        !semantic_param_publication_member_projection_syntax_current(value,
            caller, p)) return false;
    *field_out = (SemanticPublishedSelectedField){.member = value,
        .member_id = row->id, .declaration = p->declaration,
        .declaration_id = p->declaration_id, .field_id = p->field_id,
        .field_type_node = p->field_type_node, .field_type_id = p->field_type_id,
        .field_ordinal = p->field_ordinal, .field_count = p->field_count,
        .nominal_type = p->receiver_type, .field_type = p->field_type};
    if (caller_out != NULL) *caller_out = caller;
    if (root_out != NULL) *root_out = root;
    return true;
}

static Type *
expr_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved =
        semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}
static void
expr_report_unknown_member(SemanticContext *ctx, ASTNode *site,
                           const Type *object_type, const char *field_name)
{
    const char *type_name = type_name_or_unknown(object_type);
    const char *member_name = field_name != NULL ? field_name : "<member>";
    semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL,
        PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, site,
        "Unknown member '%s.%s'.\n"
        "Reason:\n"
        "- the receiver type is known, but no field or overlay member with this name is declared\n"
        "- method calls are resolved through the call path; value-position member access requires a field\n"
        "Fix:\n"
        "- declare field '%s' on '%s'\n"
        "- or call an existing method with parentheses if this was meant to be an action",
        type_name,
        member_name,
        member_name,
        type_name);
}
Type *
type_check_member_access(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *member_object = ast_member_object(expr);
    const char *member_name = ast_member_name(expr);

    if (expr_member_is_static_access(expr)) {
        char *flat_name = flatten_static_member_access(expr, '_');
        char *display_name = flatten_static_member_access(expr, '.');
        Symbol *sym = flat_name != NULL ? scope_lookup(ctx->scope, flat_name) : NULL;
        if (sym == NULL && member_name != NULL)
            sym = scope_lookup(ctx->scope, member_name);
        if (sym != NULL) {
            sym->is_used = true;
            if (semantic_param_publication_pending(ctx) &&
                !semantic_param_publication_record_value(ctx, expr, sym->type, true))
                semantic_error(ctx, expr, "Checked static value publication failed");
            if (!ctx->has_error &&
                member_name != NULL &&
                sym->kind == SYMBOL_VARIABLE && sym->type != NULL &&
                sym->type->kind == TYPE_KIND_ENUM) {
                ASTNode *qualifier = ast_member_object(expr);
                const char *qualifier_name = qualifier != NULL &&
                    qualifier->type == AST_IDENTIFIER
                    ? ast_identifier_name(qualifier) : NULL;
                Symbol *owner = qualifier_name != NULL
                    ? scope_lookup(ctx->scope, qualifier_name) : NULL;
                ASTNode *decl = semantic_host_decl_for_type(ctx, sym->type);
                size_t variant_count = 0, ordinal = 0;
                char **variants = decl != NULL && decl->type == AST_ENUM_DECL
                    ? ast_enum_variants(decl, &variant_count) : NULL;
                while (ordinal < variant_count &&
                       (variants == NULL || variants[ordinal] == NULL ||
                        strcmp(variants[ordinal], member_name) != 0))
                    ordinal++;
                if (decl != NULL && owner != NULL && owner->kind == SYMBOL_CLASS &&
                    owner->type == sym->type &&
                    owner->decl_syntax_id != 0 &&
                    owner->decl_syntax_id == sym->decl_syntax_id &&
                    owner->decl_syntax_id == ast_node_stable_id(decl) &&
                    sym->name != NULL && strcmp(sym->name, member_name) == 0 &&
                    ast_enum_name(decl) != NULL &&
                    strcmp(ast_enum_name(decl), qualifier_name) == 0 &&
                    ordinal < variant_count &&
                    ast_enum_variant_param_count(decl, ordinal) == 0 &&
                    !semantic_param_publication_record_checked_unit_enum_selection(
                        ctx, expr, decl, sym->type, ordinal))
                    semantic_error(ctx, expr,
                        "Checked unit enum selection publication failed");
            }
            free(flat_name);
            free(display_name);
            return sym->type;
        }
        semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
            "Undefined symbol '%s' (check namespace spelling or export visibility)",
            display_name != NULL ? display_name : "<member>");
        free(flat_name);
        free(display_name);
        return TYPE_UNKNOWN;
    }

    Type *object_type = type_check_expression(member_object, ctx);

    if (semantic_collection_reject_moved_member_use(expr, ctx))
        return TYPE_UNKNOWN;

    Type *projection_field = expr_ops_projection_member(expr, member_object,
        member_name, object_type, ctx);
    if (projection_field != NULL)
        return projection_field;

    if ((type_is_constructed_named(object_type, "Array")
         || type_is_constructed_named(object_type, "Slice"))
        && strcmp(member_name, "Length") == 0) {
        return TYPE_INT;
    }

    if (object_type != NULL && object_type->kind == TYPE_KIND_ENUM) {
        Type *variant_payload = expr_type_for_enum_variant_projection(ctx,
            expr, object_type, member_name);
        if (variant_payload != NULL)
            return variant_payload;
    }

    if (object_type != NULL && object_type->kind == TYPE_KIND_CLASS
        && object_type->name != NULL && strchr(object_type->name, '$') != NULL) {
        Type *payload_field = expr_type_for_enum_payload_field(ctx, expr,
            object_type, member_name);
        if (payload_field != NULL)
            return payload_field;
    }

    /* Resolve nominal/domain field types through the shared host-decl seam. */
    if (object_type != NULL
        && (object_type->kind == TYPE_KIND_CLASS
            || object_type->kind == TYPE_KIND_ENUM
            || object_type->kind == TYPE_KIND_CONSTRUCTED)
        && object_type->name != NULL) {
        const char *field_name = member_name;
        ASTNode *decl = semantic_host_decl_for_type(ctx, object_type);

        if (decl != NULL && decl->type == AST_CLASS_DECL) {
            size_t field_count = projection_source_field_count(decl);
            for (size_t fi = 0; fi < field_count; fi++) {
                PgyDeclField cf = projection_source_field_at(decl, fi);
                if (cf.name == NULL)
                    continue;
                if (strcmp(cf.name, field_name) == 0) {
                    if (!explicit_member_access_allowed(decl,
                            object_type,
                            cf.access,
                            cf.has_explicit_access,
                            ctx)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_VISIBILITY_BOUNDARY, PGY_CAUSE_VISIBILITY_BOUNDARY_CROSS, PGY_FIX_WIDEN_VISIBILITY_OR_MOVE_CALLER, expr,
                            "Member '%s.%s' is not accessible across the current visibility boundary",
                            object_type->name,
                            field_name);
                        return TYPE_UNKNOWN;
                    }
                    Type *field_type = expr_resolve_type_ref(cf.type_ast, ctx);
                    Type *result = expr_host_method_generic_substitute(
                        field_type, decl, object_type, NULL);
                    projection_record_member(ctx, expr, member_object,
                        object_type, decl, fi, field_count, cf, field_type, result);
                    return result;
                }
            }
        } else if (decl != NULL) {
            if (decl->type == AST_WORLD_DECL) {
                size_t roster_count = 0;
                ASTNode **rosters = ast_world_rosters(decl, &roster_count);
                for (size_t i = 0; i < roster_count; i++) {
                    ASTNode *slot = rosters[i];
                    const char *slot_name = ast_world_roster_slot_name(slot);
                    if (slot != NULL && slot_name != NULL
                        && strcmp(slot_name, field_name) == 0) {
                        return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                            ctx, ast_world_roster_type_name(slot), slot);
                    }
                }
                size_t zone_count = 0;
                ASTNode **zones = ast_world_zones(decl, &zone_count);
                for (size_t i = 0; i < zone_count; i++) {
                    ASTNode *slot = zones[i];
                    const char *slot_name = ast_world_zone_slot_name(slot);
                    if (slot != NULL && slot_name != NULL
                        && strcmp(slot_name, field_name) == 0) {
                        return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                            ctx, ast_world_zone_type_name(slot), slot);
                    }
                }
            }
            if (decl->type == AST_ZONE_DECL) {
                size_t layer_slot_count = 0;
                ASTNode **layer_slots = ast_zone_layer_slots(
                    decl, &layer_slot_count);
                for (size_t i = 0; i < layer_slot_count; i++) {
                    ASTNode *slot = layer_slots[i];
                    if (slot != NULL
                        && ast_zone_layer_slot_name(slot) != NULL
                        && strcmp(ast_zone_layer_slot_name(slot),
                                  field_name) == 0) {
                        return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                            ctx, ast_zone_layer_slot_layer_type(slot), slot);
                    }
                }
            }

            if (decl->type == AST_PARTY_DECL) {
                size_t party_role_count = ast_party_role_count(decl);
                for (size_t i = 0; i < party_role_count; i++) {
                    ASTNode *slot = ast_party_role(decl, i);
                    const char *party_slot_name = ast_role_slot_name(slot);
                    if (party_slot_name == NULL
                        || strcmp(party_slot_name, field_name) != 0)
                        continue;
                    if (ast_role_slot_required_ability_count(slot) > 0) {
                        ASTNode *ability_ref =
                            ast_role_slot_required_ability(slot, 0);
                        const char *ability_name = ability_ref != NULL
                            ? ast_type_name(ability_ref)
                            : NULL;
                        if (ability_name != NULL)
                            return create_overlay_nominal_type(ability_name);
                    }
                    return TYPE_UNKNOWN;
                }
            }

            for (size_t fi = 0; fi < overlay_field_count(decl); fi++) {
                const char *overlay_field_name = NULL;
                ASTNode *field_type_node = overlay_field_decl_at(decl, fi,
                    &overlay_field_name);
                if (overlay_field_name != NULL
                    && strcmp(overlay_field_name, field_name) == 0) {
                    return expr_resolve_type_ref(field_type_node, ctx);
                }
            }
        }

        expr_report_unknown_member(ctx, expr, object_type, field_name);
        return TYPE_UNKNOWN;
    }

    if (object_type != NULL
        && (object_type->kind == TYPE_KIND_CLASS
            || object_type->kind == TYPE_KIND_ENUM)) {
        expr_report_unknown_member(ctx, expr, object_type,
            member_name);
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}
