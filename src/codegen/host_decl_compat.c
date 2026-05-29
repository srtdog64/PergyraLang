/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared AST compatibility views for hosted declarations.
 */

#include "host_decl_compat.h"

#include "../parser/ast_api.h"

static const ASTNodeType kPgyHostDeclCompatTypes[] = {
    AST_CLASS_DECL,
    AST_ENUM_DECL,
    AST_PARTY_DECL,
    AST_ROLE_DECL,
    AST_ROSTER_DECL,
    AST_RELATION_DECL,
    AST_EFFECT_DECL,
    AST_ZONE_DECL,
    AST_WORLD_DECL,
};

static const ASTNodeType kPgyHostDeclCompatNominalLookupTypes[] = {
    AST_RELATION_DECL,
    AST_EFFECT_DECL,
    AST_ZONE_DECL,
    AST_WORLD_DECL,
    AST_PARTY_DECL,
    AST_ROLE_DECL,
    AST_ROSTER_DECL,
    AST_ENUM_DECL,
    AST_CLASS_DECL,
};

static const ASTNodeType kPgyHostDeclCompatConstructorDomainTypes[] = {
    AST_PARTY_DECL,
    AST_ROSTER_DECL,
    AST_RELATION_DECL,
    AST_EFFECT_DECL,
    AST_ZONE_DECL,
    AST_WORLD_DECL,
};

const ASTNodeType *
pgy_host_decl_compat_types(size_t *count_out)
{
    if (count_out != NULL) {
        *count_out = sizeof(kPgyHostDeclCompatTypes)
            / sizeof(kPgyHostDeclCompatTypes[0]);
    }
    return kPgyHostDeclCompatTypes;
}

const ASTNodeType *
pgy_host_decl_compat_nominal_lookup_types(size_t *count_out)
{
    if (count_out != NULL) {
        *count_out = sizeof(kPgyHostDeclCompatNominalLookupTypes)
            / sizeof(kPgyHostDeclCompatNominalLookupTypes[0]);
    }
    return kPgyHostDeclCompatNominalLookupTypes;
}

const ASTNodeType *
pgy_host_decl_compat_constructor_domain_types(size_t *count_out)
{
    if (count_out != NULL) {
        *count_out = sizeof(kPgyHostDeclCompatConstructorDomainTypes)
            / sizeof(kPgyHostDeclCompatConstructorDomainTypes[0]);
    }
    return kPgyHostDeclCompatConstructorDomainTypes;
}

bool
pgy_host_decl_compat_is_type(ASTNodeType decl_type)
{
    size_t count = 0;
    const ASTNodeType *types = pgy_host_decl_compat_types(&count);

    for (size_t i = 0; types != NULL && i < count; i++) {
        if (types[i] == decl_type)
            return true;
    }
    return false;
}

const char *
pgy_host_decl_compat_name(ASTNode *decl)
{
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        return ast_class_name(decl);
    case AST_ENUM_DECL:
        return ast_enum_name(decl);
    case AST_PARTY_DECL:
        return ast_party_name(decl);
    case AST_ROLE_DECL:
        return ast_role_name(decl);
    case AST_ROSTER_DECL:
        return ast_roster_name(decl);
    case AST_RELATION_DECL:
        return ast_relation_name(decl);
    case AST_EFFECT_DECL:
        return ast_effect_name(decl);
    case AST_ZONE_DECL:
        return ast_zone_name(decl);
    case AST_WORLD_DECL:
        return ast_world_name(decl);
    default:
        return NULL;
    }
}

bool
pgy_host_decl_compat_uses_pointer_self(ASTNode *decl)
{
    if (decl == NULL)
        return false;

    switch (decl->type) {
    case AST_PARTY_DECL:
    case AST_ROLE_DECL:
    case AST_ROSTER_DECL:
    case AST_WORLD_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_ZONE_DECL:
        return true;
    case AST_CLASS_DECL:
        return ast_class_nominal_kind(decl) == NOMINAL_DECL_SUBJECT
            || ast_class_nominal_kind(decl) == NOMINAL_DECL_VESSEL;
    default:
        return false;
    }
}

bool
pgy_host_decl_compat_has_projection_ready_flag(ASTNode *decl)
{
    if (decl == NULL)
        return false;

    switch (decl->type) {
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_ZONE_DECL:
        return true;
    default:
        return false;
    }
}

PgyHostMethodCompatView
pgy_host_method_compat_view_from_decl(ASTNode *decl,
                                      bool require_role_method_total)
{
    PgyHostMethodCompatView view;

    view.methods = NULL;
    view.count = 0;
    if (decl == NULL)
        return view;

    switch (decl->type) {
    case AST_CLASS_DECL:
        view.methods = ast_class_methods(decl, &view.count);
        break;
    case AST_ENUM_DECL:
        view.methods = ast_enum_methods(decl, &view.count);
        break;
    case AST_PARTY_DECL:
        view.methods = ast_party_methods(decl, &view.count);
        break;
    case AST_ROSTER_DECL:
        view.methods = ast_roster_methods(decl, &view.count);
        break;
    case AST_ROLE_DECL:
        if (require_role_method_total
            && !ast_role_impl_method_total_count(decl, &view.count)) {
            view.count = (size_t)-1;
        }
        break;
    case AST_WORLD_DECL:
        view.methods = ast_world_methods(decl, &view.count);
        break;
    case AST_RELATION_DECL:
        view.methods = ast_relation_methods(decl, &view.count);
        break;
    case AST_EFFECT_DECL:
        view.methods = ast_effect_methods(decl, &view.count);
        break;
    case AST_ZONE_DECL:
        view.methods = ast_zone_methods(decl, &view.count);
        break;
    default:
        break;
    }

    return view;
}

PgyHostSharedFieldsCompatView
pgy_host_shared_fields_compat_view_from_decl(ASTNode *decl)
{
    PgyHostSharedFieldsCompatView view;

    view.fields = NULL;
    view.count = 0;
    if (decl == NULL)
        return view;

    switch (decl->type) {
    case AST_PARTY_DECL:
        view.fields = ast_party_shared_fields(decl, &view.count);
        break;
    case AST_ROSTER_DECL:
        view.fields = ast_roster_shared_fields(decl, &view.count);
        break;
    case AST_RELATION_DECL:
        view.fields = ast_relation_shared_fields(decl, &view.count);
        break;
    case AST_EFFECT_DECL:
        view.fields = ast_effect_shared_fields(decl, &view.count);
        break;
    case AST_ZONE_DECL:
        view.fields = ast_zone_shared_fields(decl, &view.count);
        break;
    case AST_WORLD_DECL:
        view.fields = ast_world_shared_fields(decl, &view.count);
        break;
    default:
        break;
    }

    return view;
}

PgyHostClassFieldsCompatView
pgy_host_class_fields_compat_view_from_decl(ASTNode *decl)
{
    PgyHostClassFieldsCompatView view;

    view.fields = NULL;
    view.count = 0;
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return view;

    view.fields = ast_class_fields(decl, &view.count);
    return view;
}
