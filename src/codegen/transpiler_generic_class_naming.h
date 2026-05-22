#ifndef PGY_TRANSPILER_GENERIC_CLASS_NAMING_H
#define PGY_TRANSPILER_GENERIC_CLASS_NAMING_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

bool transpiler_generic_class_copy_name(char *out, size_t out_size,
                                        const char *name);
bool transpiler_generic_class_method_name(char *out, size_t out_size,
                                          const char *class_name,
                                          const char *method_name);
bool transpiler_generic_class_surface_desc(char *out, size_t out_size,
                                           const char *surface_kind,
                                           const char *class_name,
                                           const char *method_name,
                                           const char *param_name);
void transpiler_generic_class_format_too_long(TranspilerCtx *ctx,
                                              const char *surface_kind);
char *transpiler_generic_class_specialization_name(ASTNode *class_decl,
                                                   ASTNode *ann,
                                                   bool *has_effective_args);

#endif /* PGY_TRANSPILER_GENERIC_CLASS_NAMING_H */
