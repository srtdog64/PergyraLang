#ifndef PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_NAMES_H
#define PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_NAMES_H

#include <stdbool.h>
#include <stddef.h>

bool transpiler_role_ability_copy_name(char *out,
                                       size_t out_size,
                                       const char *name);
bool transpiler_role_ability_host_method_name(char *out,
                                              size_t out_size,
                                              const char *host_name,
                                              const char *method_name);
bool transpiler_role_ability_vtable_typedef_name(char *out,
                                                 size_t out_size,
                                                 const char *tag);
bool transpiler_role_operator_alias_name(char *out,
                                         size_t out_size,
                                         const char *suffix,
                                         const char *for_type);
bool transpiler_role_ability_surface_desc(char *out,
                                          size_t out_size,
                                          const char *prefix,
                                          const char *owner_name,
                                          const char *method_name,
                                          const char *param_name);

#endif /* PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_NAMES_H */
