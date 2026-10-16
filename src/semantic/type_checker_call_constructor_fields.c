#include "type_checker_internal.h"
#include "type_checker_call_constructor_internal.h"
#include "function_param_publication_owner.h"
#include "diag_codes.h"

#include <stdlib.h>
#include <string.h>

bool
domain_constructor_has_role_filtered_inputs(ASTNode *decl)
{
    return decl != NULL
        && (decl->type == AST_ZONE_DECL
            || decl->type == AST_RELATION_DECL
            || decl->type == AST_EFFECT_DECL);
}

static bool
domain_constructor_slot_is_input(ASTNode *slot)
{
    return slot != NULL && slot->type == AST_DOMAIN_SLOT
        && (ast_domain_slot_is_subject(slot)
            || ast_domain_slot_is_binding(slot));
}

static ASTNode **
domain_constructor_slots(ASTNode *decl, size_t *slot_count_out)
{
    if (slot_count_out != NULL)
        *slot_count_out = 0;
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_ZONE_DECL)
        return ast_zone_slots(decl, slot_count_out);
    if (decl->type == AST_RELATION_DECL)
        return ast_relation_slots(decl, slot_count_out);
    if (decl->type == AST_EFFECT_DECL)
        return ast_effect_slots(decl, slot_count_out);
    return NULL;
}

ASTNode *
domain_constructor_input(ASTNode *decl, size_t index, size_t *count)
{
    size_t slot_count = 0;
    size_t input_count = 0;
    ASTNode *selected = NULL;
    ASTNode **slots = domain_constructor_slots(decl, &slot_count);
    for (size_t i = 0; i < slot_count; i++) {
        if (!domain_constructor_slot_is_input(slots[i]))
            continue;
        if (input_count++ == index)
            selected = slots[i];
    }
    if (count != NULL)
        *count = input_count;
    return selected;
}

static ASTNode *
constructor_decl_field_type_at(ASTNode *decl,
                               size_t index,
                               const char **field_name_out)
{
    if (field_name_out != NULL)
        *field_name_out = NULL;
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        PgyDeclField field = subject_host_field_at(decl, index);
        if (field_name_out != NULL && field.name != NULL)
            *field_name_out = field.name;
        return field.name != NULL ? field.type_ast : NULL;
    }
    if (domain_constructor_has_role_filtered_inputs(decl)) {
        ASTNode *slot = domain_constructor_input(decl, index, NULL);
        if (field_name_out != NULL)
            *field_name_out = ast_domain_slot_name(slot);
        return ast_domain_slot_type(slot);
    }
    return overlay_field_decl_at(decl, index, field_name_out);
}

Type *
constructor_decl_field_resolved_type_at(ASTNode *decl, size_t index,
                                        const char **field_name_out,
                                        SemanticContext *ctx)
{
    /* World roster/zone bindings own type names, not AST_TYPE nodes. */
    if (decl != NULL && decl->type == AST_WORLD_DECL) {
        size_t roster_count = 0, zone_count = 0;
        ASTNode **rosters = ast_world_rosters(decl, &roster_count);
        ASTNode **zones = ast_world_zones(decl, &zone_count);
        ASTNode *binding = NULL;
        const char *name = NULL, *type_name = NULL;
        if (index < roster_count) {
            binding = rosters[index];
            name = ast_world_roster_slot_name(binding);
            type_name = ast_world_roster_type_name(binding);
        } else if (index - roster_count < zone_count) {
            binding = zones[index - roster_count];
            name = ast_world_zone_slot_name(binding);
            type_name = ast_world_zone_type_name(binding);
        }
        if (index < roster_count + zone_count) {
            if (field_name_out != NULL)
                *field_name_out = name;
            return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                ctx, type_name, binding);
        }
    }
    ASTNode *type_node = constructor_decl_field_type_at(decl, index, field_name_out);
    return type_node != NULL ? semantic_host_resolve_type_ref(type_node, ctx) : NULL;
}

ASTNode *
constructor_decl_field_type_by_name(ASTNode *decl, const char *name,
                                    size_t field_count,
                                    const char **field_name_out, size_t *selected)
{
    for (size_t i = 0; i < field_count; i++) {
        const char *fn = NULL;
        ASTNode *ft = constructor_decl_field_type_at(decl, i, &fn);
        if (fn != NULL && name != NULL && strcmp(fn, name) == 0) {
            *selected = i;
            if (field_name_out != NULL)
                *field_name_out = fn;
            return ft;
        }
    }
    if (field_name_out != NULL)
        *field_name_out = NULL;
    return NULL;
}

bool
constructor_decl_has_channel_field(ASTNode *decl,
                                   SemanticContext *ctx,
                                   const char **field_name_out)
{
    size_t field_count;

    if (field_name_out != NULL)
        *field_name_out = NULL;
    if (decl == NULL)
        return false;
    field_count = decl->type == AST_CLASS_DECL
        ? projection_source_field_count(decl)
        : overlay_field_count(decl);
    for (size_t i = 0; i < field_count; i++) {
        const char *field_name = NULL;
        ASTNode *field_type_node = decl->type == AST_CLASS_DECL
            ? constructor_decl_field_type_at(decl, i, &field_name)
            : overlay_field_decl_at(decl, i, &field_name);
        Type *field_type;
        if (field_type_node == NULL)
            continue;
        field_type = semantic_host_resolve_type_ref(field_type_node, ctx);
        if (field_type_node->type != AST_CHANNEL_TYPE &&
            !type_is_constructed_named(field_type, "Channel")) {
            continue;
        }
        if (field_name_out != NULL)
            *field_name_out = field_name;
        return true;
    }
    return false;
}

void
constructor_reject_channel_field_store(SemanticContext *ctx,
                                       ASTNode *site,
                                       const char *constructor_name,
                                       const char *field_name)
{
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_CHANNEL_TRANSPORT_INVALID,
        PGY_CAUSE_CHANNEL_TRANSPORT_RULE_VIOLATION,
        PGY_FIX_PROVIDE_MOVABLE_HANDLE,
        site,
        "Constructor '%s' cannot aggregate-construct or default-initialize Channel<T> field '%s'.\n"
        "Reason:\n"
        "- ordinary Channel<T> currently lowers to runtime storage with synchronization state\n"
        "- copying that storage would duplicate mutex/condition-variable ownership\n"
        "- default-zeroing that storage would bypass channel runtime initialization\n"
        "- C and LLVM backends require a movable channel-handle lowering before this is safe\n"
        "Fix:\n"
        "- keep the channel in a local binding and pass values through send/recv\n"
        "- or wait for the movable Channel<T> handle ABI before storing channels in aggregate fields",
        constructor_name != NULL ? constructor_name : "<constructor>",
        field_name != NULL ? field_name : "<field>");
}

bool constructor_payload_type_ready(const Type *type)
{
    if (type == NULL || type == TYPE_UNKNOWN || type == TYPE_VOID ||
        type->name == NULL || type->kind == TYPE_KIND_GENERIC ||
        type->kind == TYPE_KIND_FUNCTION || type->nominal_flavor == TYPE_NOMINAL_SUBJECT ||
        type_is_resource_handle(type) || type_is_move_token(type) ||
        type_is_capability_bearing(type) || semantic_type_is_future_handle(type)) return false;
    if (type->kind == TYPE_KIND_CONSTRUCTED)
        for (size_t i = 0; i < type_constructed_arg_count(type); i++)
            if (!constructor_payload_type_ready(type_constructed_arg(type, i))) return false;
    if (type->kind == TYPE_KIND_TUPLE)
        for (size_t i = 0; i < type->data.tuple.element_count; i++)
            if (!constructor_payload_type_ready(type->data.tuple.elements[i])) return false;
    return true;
}

bool semantic_nominal_result_snapshot_current(const SemanticContext *ctx,
    const ASTNode *call, const SemanticNominalResultSnapshot *snapshot)
{
    const char *result_name = ast_call_semantic_value_type_name(call);
    if (snapshot == NULL || snapshot->decl_name == NULL || snapshot->result_type_name == NULL ||
        result_name == NULL || strcmp(result_name, snapshot->result_type_name) != 0 ||
        (snapshot->field_count != 0 && snapshot->fields == NULL) || !semantic_param_publication_inputs_ready(ctx) ||
        semantic_host_index_find_decl_by_name((SemanticContext *)ctx, AST_CLASS_DECL,
            snapshot->decl_name) != snapshot->decl ||
        ast_node_stable_id(snapshot->decl) != snapshot->decl_id ||
        !ast_class_is_struct(snapshot->decl) ||
        ast_generic_param_count(ast_class_generic_params(snapshot->decl)) != 0 ||
        ast_class_field_destructure_count(snapshot->decl) != 0 ||
        ast_class_name(snapshot->decl) == NULL ||
        strcmp(ast_class_name(snapshot->decl), snapshot->decl_name) != 0 ||
        ast_call_arg_count(call) != snapshot->field_count) return false;
    size_t count = 0;
    ClassField **fields = ast_class_fields(snapshot->decl, &count);
    if (count != snapshot->field_count || (count != 0 && fields == NULL)) return false;
    for (size_t i = 0; i < count; i++) {
        const SemanticNominalResultField *row = &snapshot->fields[i];
        const char *name = ast_call_argument_name(call, i);
        if (row->field == NULL || row->field_name == NULL || row->field_type_name == NULL ||
            row->argument_type_name == NULL || row->type_syntax_name == NULL ||
            row->field_index >= count || fields[row->field_index] != row->field ||
            row->field->stable_id != row->field_id || row->field->default_value != NULL ||
            row->field->type != row->type_node || ast_node_stable_id(row->type_node) != row->type_id ||
            row->field->name == NULL || strcmp(row->field->name, row->field_name) != 0 ||
            (name == NULL) != (row->argument_name == NULL) ||
            (name != NULL && strcmp(name, row->argument_name) != 0)) return false;
        Type *type = semantic_host_resolve_type_ref(row->type_node, (SemanticContext *)ctx);
        if (!constructor_payload_type_ready(type) || strcmp(type->name, row->field_type_name) != 0) return false;
        char *syntax = ast_capture_inline(row->type_node);
        bool same = syntax != NULL && strcmp(syntax, row->type_syntax_name) == 0;
        free(syntax);
        if (!same) return false;
    }
    return true;
}
