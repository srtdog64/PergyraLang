#ifndef PERGYRA_MIR_SSA_RENAME_INTERNAL_H
#define PERGYRA_MIR_SSA_RENAME_INTERNAL_H

#include "mir.h"

bool mir_collect_ssa_names(const MIRRoutine *routine,
                           MIRLocalBinding **names_out,
                           size_t *count_out);
int mir_find_ssa_binding_index(const MIRLocalBinding *names, size_t count,
                                uint32_t binding_syntax_id);
bool mir_append_ssa_binding(MIRLocalBinding **rows, size_t *count,
                            size_t *capacity, MIRLocalBinding binding);
bool mir_collect_expr_identifier_uses(ASTNode *node,
                                      MIRLocalBinding **uses,
                                      size_t *use_count,
                                      size_t *use_capacity);

#endif
