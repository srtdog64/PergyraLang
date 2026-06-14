#include "mir_decl_header_shape.h"

#include "../parser/ast_api.h"

size_t
mir_decl_header_ast_domain_method_count(ASTNode *ast)
{
    size_t method_count = 0;

    if (ast == NULL)
        return 0;
    switch (ast->type) {
    case AST_WORLD_DECL:
        (void) ast_world_methods(ast, &method_count);
        break;
    case AST_RELATION_DECL:
        (void) ast_relation_methods(ast, &method_count);
        break;
    case AST_EFFECT_DECL:
        (void) ast_effect_methods(ast, &method_count);
        break;
    case AST_ZONE_DECL:
        (void) ast_zone_methods(ast, &method_count);
        break;
    default:
        break;
    }
    return method_count;
}

size_t
mir_decl_header_ast_field_count(ASTNode *ast)
{
    size_t count = 0;
    size_t extra = 0;

    if (ast == NULL)
        return 0;
    switch (ast->type) {
    case AST_CLASS_DECL:
        (void) ast_class_fields(ast, &count);
        return count;
    case AST_PARTY_DECL:
        return ast_party_role_count(ast) + ast_party_shared_count(ast);
    case AST_ROSTER_DECL:
        return ast_roster_party_count(ast) + ast_roster_shared_count(ast);
    case AST_WORLD_DECL:
        (void) ast_world_rosters(ast, &count);
        (void) ast_world_zones(ast, &extra);
        count += extra;
        (void) ast_world_shared_fields(ast, &extra);
        return count + extra;
    case AST_RELATION_DECL:
        (void) ast_relation_slots(ast, &count);
        (void) ast_relation_shared_fields(ast, &extra);
        return count + extra;
    case AST_EFFECT_DECL:
        (void) ast_effect_slots(ast, &count);
        (void) ast_effect_shared_fields(ast, &extra);
        return count + extra;
    case AST_ZONE_DECL:
        (void) ast_zone_slots(ast, &count);
        (void) ast_zone_layer_slots(ast, &extra);
        count += extra;
        (void) ast_zone_shared_fields(ast, &extra);
        return count + extra;
    default:
        return 0;
    }
}

bool
mir_decl_header_ast_shape(const MIRDeclHeader *header,
                          const char **name_out,
                          size_t *generic_count_out,
                          size_t *method_count_out,
                          size_t *field_count_out,
                          bool *uses_pointer_self_out)
{
    ASTNode *ast;

    if (name_out != NULL)
        *name_out = NULL;
    if (generic_count_out != NULL)
        *generic_count_out = 0;
    if (method_count_out != NULL)
        *method_count_out = 0;
    if (field_count_out != NULL)
        *field_count_out = 0;
    if (uses_pointer_self_out != NULL)
        *uses_pointer_self_out = false;
    if (header == NULL)
        return false;
    ast = header->source_ast;
    if (ast == NULL)
        return false;
    if (generic_count_out != NULL) {
        GenericParams *params = ast_declaration_generic_params(ast);
        *generic_count_out = ast_generic_param_count(params);
    }

    switch (ast->type) {
    case AST_FUNC_DECL:
        if (name_out != NULL)
            *name_out = ast_declaration_name(ast);
        return true;
    case AST_TYPE_ALIAS:
        if (name_out != NULL)
            *name_out = ast_type_alias_name(ast);
        return true;
    case AST_CLASS_DECL:
        if (name_out != NULL)
            *name_out = ast_class_name(ast);
        if (method_count_out != NULL)
            (void) ast_class_methods(ast, method_count_out);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_header_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out =
                ast_class_nominal_kind(ast) == NOMINAL_DECL_SUBJECT
                || ast_class_nominal_kind(ast) == NOMINAL_DECL_VESSEL;
        return true;
    case AST_ENUM_DECL:
        if (name_out != NULL)
            *name_out = ast_enum_name(ast);
        if (method_count_out != NULL)
            (void) ast_enum_methods(ast, method_count_out);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_header_ast_field_count(ast);
        return true;
    case AST_ABILITY_DECL:
        if (name_out != NULL)
            *name_out = ast_ability_name(ast);
        return true;
    case AST_EVENT_DECL:
        if (name_out != NULL)
            *name_out = ast_event_name(ast);
        return true;
    case AST_INTENT_DECL:
        if (name_out != NULL)
            *name_out = ast_intent_decl_name(ast);
        return true;
    case AST_PARTY_DECL:
        if (name_out != NULL)
            *name_out = ast_party_name(ast);
        if (method_count_out != NULL)
            *method_count_out = ast_party_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_header_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ROLE_DECL:
        if (name_out != NULL)
            *name_out = ast_role_name(ast);
        if (method_count_out != NULL
            && !ast_role_impl_method_total_count(ast, method_count_out)) {
            return false;
        }
        if (field_count_out != NULL)
            *field_count_out = mir_decl_header_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ROSTER_DECL:
        if (name_out != NULL)
            *name_out = ast_roster_name(ast);
        if (method_count_out != NULL)
            *method_count_out = ast_roster_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_header_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_WORLD_DECL:
        if (name_out != NULL)
            *name_out = ast_world_name(ast);
        if (method_count_out != NULL)
            *method_count_out = mir_decl_header_ast_domain_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_header_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_RELATION_DECL:
        if (name_out != NULL)
            *name_out = ast_relation_name(ast);
        if (method_count_out != NULL)
            *method_count_out = mir_decl_header_ast_domain_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_header_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_EFFECT_DECL:
        if (name_out != NULL)
            *name_out = ast_effect_name(ast);
        if (method_count_out != NULL)
            *method_count_out = mir_decl_header_ast_domain_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_header_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ZONE_DECL:
        if (name_out != NULL)
            *name_out = ast_zone_name(ast);
        if (method_count_out != NULL)
            *method_count_out = mir_decl_header_ast_domain_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_header_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    default:
        return false;
    }
}
