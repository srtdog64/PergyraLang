#ifndef PERGYRA_TRANSPILER_ROLE_ABILITY_HELPERS_H
#define PERGYRA_TRANSPILER_ROLE_ABILITY_HELPERS_H

#include "transpiler.h"

bool role_has_ability(ASTNode *role, const char *ability_name);
bool role_has_method(ASTNode *role, const char *method_name);

static char *
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

#endif /* PERGYRA_TRANSPILER_ROLE_ABILITY_HELPERS_H */
