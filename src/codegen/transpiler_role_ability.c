#include "transpiler.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
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
render_ability_ref_vtable_tag(ASTNode *ability_ref)
{
    char suffix[128];
    size_t len;
    char *rendered;

    if (ability_ref == NULL)
        return NULL;

    rendered = render_type_name(ability_ref);
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
