#include "transpiler_domain_role_ability_names.h"

#include <stdio.h>
#include <string.h>

bool
transpiler_role_ability_copy_name(char *out, size_t out_size, const char *name)
{
    size_t len;

    if (out == NULL || out_size == 0 || name == NULL)
        return false;
    len = strlen(name);
    if (len >= out_size)
        return false;
    memcpy(out, name, len + 1);
    return true;
}

bool
transpiler_role_ability_host_method_name(char *out,
                                         size_t out_size,
                                         const char *host_name,
                                         const char *method_name)
{
    int written;

    if (out == NULL || out_size == 0 || host_name == NULL
        || method_name == NULL) {
        return false;
    }
    written = snprintf(out, out_size, "%s_%s", host_name, method_name);
    return written >= 0 && (size_t)written < out_size;
}

bool
transpiler_role_ability_vtable_typedef_name(char *out,
                                            size_t out_size,
                                            const char *tag)
{
    int written;

    if (out == NULL || out_size == 0 || tag == NULL)
        return false;
    written = snprintf(out, out_size, "%s_vtable", tag);
    return written >= 0 && (size_t)written < out_size;
}

bool
transpiler_role_operator_alias_name(char *out,
                                    size_t out_size,
                                    const char *suffix,
                                    const char *for_type)
{
    int written;

    if (out == NULL || out_size == 0 || suffix == NULL || for_type == NULL)
        return false;
    written = snprintf(out, out_size, "operator_%s_%s", suffix, for_type);
    return written >= 0 && (size_t)written < out_size;
}

bool
transpiler_role_ability_surface_desc(char *out,
                                     size_t out_size,
                                     const char *prefix,
                                     const char *owner_name,
                                     const char *method_name,
                                     const char *param_name)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL)
        return false;
    written = snprintf(out, out_size, "%s '%s.%s(%s)'",
        prefix,
        owner_name != NULL ? owner_name : "(anonymous)",
        method_name != NULL ? method_name : "(anonymous)",
        param_name != NULL ? param_name : "(anonymous)");
    return written >= 0 && (size_t)written < out_size;
}
