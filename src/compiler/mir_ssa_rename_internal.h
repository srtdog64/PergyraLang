#ifndef PERGYRA_MIR_SSA_RENAME_INTERNAL_H
#define PERGYRA_MIR_SSA_RENAME_INTERNAL_H

#include "mir.h"

bool mir_collect_ssa_names(const MIRRoutine *routine,
                           const char ***names_out,
                           size_t *count_out);
int mir_find_ssa_name_index(const char **names, size_t count, const char *name);
bool mir_collect_expr_identifier_uses(ASTNode *node,
                                      const char ***uses,
                                      size_t *use_count,
                                      size_t *use_capacity);

#endif
