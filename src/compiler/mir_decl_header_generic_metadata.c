#include "mir_decl_header_generic_metadata.h"

#include "mir_type_helpers.h"

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../parser/ast_domain_api.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static WhereClause *
mir_decl_where_clause(ASTNode *decl)
{
    if (decl == NULL)
        return NULL;
    switch (decl->type) {
    case AST_ABILITY_DECL:
        return ast_ability_where_clause(decl);
    case AST_ROLE_DECL:
        return ast_role_where_clause(decl);
    case AST_CLASS_DECL:
        return ast_class_where_clause(decl);
    default:
        return NULL;
    }
}

static bool
mir_append_owned_text(char **dst, const char *suffix)
{
    size_t dst_len;
    size_t suffix_len;
    char *grown;

    if (dst == NULL || *dst == NULL || suffix == NULL)
        return false;
    dst_len = strlen(*dst);
    suffix_len = strlen(suffix);
    if (suffix_len > (size_t)-1 - dst_len - 1)
        return false;
    grown = realloc(*dst, dst_len + suffix_len + 1);
    if (grown == NULL)
        return false;
    memcpy(grown + dst_len, suffix, suffix_len + 1);
    *dst = grown;
    return true;
}

static bool
mir_capture_where_constraint(WhereClause *where,
                             const char *param_name,
                             char **constraint_out)
{
    if (constraint_out == NULL)
        return false;
    *constraint_out = NULL;
    if (where == NULL || param_name == NULL)
        return true;

    for (size_t i = 0; i < where->count; i++) {
        TypeConstraint *constraint = where->constraints[i];
        char *rendered;

        if (constraint == NULL || constraint->type_param == NULL
            || strcmp(constraint->type_param, param_name) != 0) {
            continue;
        }
        if (constraint->bound_count == 0)
            return false;
        rendered = pergyra_strdup("");
        if (rendered == NULL)
            return false;
        for (size_t bound = 0; bound < constraint->bound_count; bound++) {
            char *bound_name;

            if (constraint->bounds[bound] == NULL) {
                free(rendered);
                return false;
            }
            if (bound > 0 && !mir_append_owned_text(&rendered, " + ")) {
                free(rendered);
                return false;
            }
            bound_name = mir_render_type_name(constraint->bounds[bound]);
            if (bound_name == NULL
                || !mir_append_owned_text(&rendered, bound_name)) {
                free(bound_name);
                free(rendered);
                return false;
            }
            free(bound_name);
        }
        *constraint_out = rendered;
        return true;
    }
    return true;
}

static void
mir_decl_generic_metadata_clear(MIRDeclGenericParam *metadata, size_t count)
{
    if (metadata == NULL)
        return;
    for (size_t i = 0; i < count; i++) {
        free(metadata[i].bound_type_name);
        free(metadata[i].default_arg_type_name);
        metadata[i].bound_type_name = NULL;
        metadata[i].default_arg_type_name = NULL;
    }
}

void
mir_decl_header_free_generics(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    mir_decl_generic_metadata_clear(
        header->generic_metadata, header->generic_metadata_count);
    free(header->generic_metadata);
    header->generic_metadata = NULL;
    header->generic_metadata_count = 0;
    header->generic_param_count = 0;
}

bool
mir_decl_header_set_generics(MIRDeclHeader *header, ASTNode *decl)
{
    GenericParams *params;
    WhereClause *where;
    size_t count;

    if (header == NULL)
        return false;

    params = ast_declaration_generic_params(decl);
    where = mir_decl_where_clause(decl);
    count = ast_generic_param_count(params);
    header->generic_param_count = count;
    header->generic_metadata = NULL;
    header->generic_metadata_count = 0;

    if (count == 0)
        return true;
    if (count > SIZE_MAX / sizeof(MIRDeclGenericParam))
        return false;
    header->generic_metadata = calloc(count, sizeof(MIRDeclGenericParam));
    if (header->generic_metadata == NULL)
        return false;

    for (size_t i = 0; i < count; i++) {
        GenericParam *param = ast_generic_param_at(params, i);
        MIRDeclGenericParam *meta = &header->generic_metadata[i];
        ASTNode *constraint;
        ASTNode *default_type;
        char *where_constraint = NULL;

        if (param == NULL) {
            mir_decl_header_free_generics(header);
            return false;
        }

        constraint = ast_generic_param_constraint(param);
        default_type = ast_generic_param_default_type(param);
        meta->name = ast_generic_param_name(param);
        meta->bound_type_name = mir_capture_type_name(constraint, NULL);
        if (meta->bound_type_name == NULL
            && !mir_capture_where_constraint(
                where, meta->name, &where_constraint)) {
            mir_decl_header_free_generics(header);
            return false;
        }
        if (meta->bound_type_name == NULL) {
            meta->bound_type_name = where_constraint;
            where_constraint = NULL;
        }
        free(where_constraint);
        meta->default_arg_type_name =
            mir_capture_type_name(default_type, NULL);
        if (meta->name == NULL
            || (constraint != NULL && meta->bound_type_name == NULL)
            || (default_type != NULL
                && meta->default_arg_type_name == NULL)) {
            mir_decl_header_free_generics(header);
            return false;
        }
    }
    header->generic_metadata_count = count;
    return true;
}
