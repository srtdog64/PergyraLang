#include "transpiler.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_role_ability_helpers.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

bool
role_has_ability(ASTNode *role, const char *ability_name)
{
    if (role == NULL || role->type != AST_ROLE_DECL || ability_name == NULL)
        return false;

    for (size_t i = 0; i < ast_role_impl_count(role); i++) {
        ASTNode *impl = ast_role_impl(role, i);
        if (impl != NULL
            && impl->type == AST_IMPL_ABILITY
            && ast_impl_ability_name(impl) != NULL
            && strcmp(ast_impl_ability_name(impl), ability_name) == 0) {
            return true;
        }
    }

    return false;
}

char *
render_ability_ref_vtable_tag_in_ctx(TranspilerCtx *ctx, ASTNode *ability_ref)
{
    char suffix[128];
    size_t len;
    char *rendered;

    if (ability_ref == NULL)
        return NULL;

    rendered = render_type_name_in_ctx(ctx, ability_ref);
    if (rendered == NULL)
        return NULL;
    sanitize_c_suffix(rendered, suffix, sizeof(suffix));
    len = strlen(suffix);
    while (len > 0 && suffix[len - 1] == '_')
        suffix[--len] = '\0';
    if (len == 0) {
        free(rendered);
        return NULL;
    }
    free(rendered);
    return pergyra_strdup(suffix);
}

char *
render_ability_ref_vtable_tag(ASTNode *ability_ref)
{
    return render_ability_ref_vtable_tag_in_ctx(NULL, ability_ref);
}

bool
role_has_method(ASTNode *role, const char *method_name)
{
    if (role == NULL || role->type != AST_ROLE_DECL || method_name == NULL)
        return false;

    for (size_t i = 0; i < ast_role_impl_count(role); i++) {
        ASTNode *impl = ast_role_impl(role, i);
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++) {
            ASTNode *method = ast_impl_ability_method(impl, j);
            const char *candidate_name = ast_declaration_name(method);
            if (method != NULL
                && method->type == AST_FUNC_DECL
                && candidate_name != NULL
                && strcmp(candidate_name, method_name) == 0) {
                return true;
            }
        }
    }

    return false;
}

char *
transpiler_party_slot_first_ability_tag(TranspilerCtx *ctx,
                                        ASTNode *party_decl,
                                        const char *slot_name)
{
    const char *party_name;
    TranspilerHostedRoleSlotView role_view;

    if (ctx == NULL || party_decl == NULL || party_decl->type != AST_PARTY_DECL
        || slot_name == NULL) {
        return NULL;
    }

    party_name = transpiler_decl_name_local(party_decl);
    role_view =
        transpiler_hosted_role_slot_view_from_decl(ctx, party_name, party_decl);
    if (transpiler_hosted_role_slot_view_missing_mir_metadata(&role_view))
        return NULL;

    for (size_t i = 0; i < role_view.count; i++) {
        const char *role_slot_name =
            transpiler_hosted_role_slot_view_name(&role_view, i);
        if (role_slot_name == NULL
            || strcmp(role_slot_name, slot_name) != 0) {
            continue;
        }
        ASTNode *first_ability =
            transpiler_hosted_role_slot_view_required_ability(&role_view, i, 0);
        if (first_ability == NULL)
            return NULL;
        return render_ability_ref_vtable_tag(first_ability);
    }

    return NULL;
}

char *
transpiler_party_slot_method_ability_tag(TranspilerCtx *ctx,
                                         ASTNode *party_decl,
                                         const char *slot_name,
                                         const char *method_name)
{
    char *fallback_tag = NULL;
    const char *party_name;
    TranspilerHostedRoleSlotView role_view;

    if (ctx == NULL || party_decl == NULL || party_decl->type != AST_PARTY_DECL
        || slot_name == NULL || method_name == NULL) {
        return NULL;
    }

    party_name = transpiler_decl_name_local(party_decl);
    role_view =
        transpiler_hosted_role_slot_view_from_decl(ctx, party_name, party_decl);
    if (transpiler_hosted_role_slot_view_missing_mir_metadata(&role_view))
        return NULL;

    for (size_t i = 0; i < role_view.count; i++) {
        const char *role_slot_name =
            transpiler_hosted_role_slot_view_name(&role_view, i);
        size_t ability_count;
        if (role_slot_name == NULL
            || strcmp(role_slot_name, slot_name) != 0) {
            continue;
        }
        ability_count =
            transpiler_hosted_role_slot_view_required_ability_count(
                &role_view, i);

        for (size_t j = 0; j < ability_count; j++) {
            ASTNode *ability_ref =
                transpiler_hosted_role_slot_view_required_ability(
                    &role_view, i, j);
            ASTNode *ability_decl;
            bool has_method = false;
            char *ability_tag;

            if (ability_ref == NULL || ast_type_name(ability_ref) == NULL)
                continue;
            ability_decl = find_ability_decl(ctx, ast_type_name(ability_ref));
            if (ability_decl != NULL) {
                for (size_t mi = 0;
                     mi < ast_ability_method_count(ability_decl); mi++) {
                    ASTNode *method = ast_ability_method(ability_decl, mi);
                    const char *candidate_name = ast_declaration_name(method);
                    if (method != NULL && method->type == AST_FUNC_DECL
                        && candidate_name != NULL
                        && strcmp(candidate_name, method_name) == 0) {
                        has_method = true;
                        break;
                    }
                }
            }

            ability_tag = render_ability_ref_vtable_tag_in_ctx(ctx, ability_ref);
            if (has_method) {
                free(fallback_tag);
                return ability_tag;
            }
            if (fallback_tag == NULL) {
                fallback_tag = ability_tag;
            } else {
                free(ability_tag);
            }
        }
        break;
    }

    return fallback_tag;
}
