/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR SSA classification helpers.
 */

#ifndef PERGYRA_TRANSPILER_MIR_SSA_UTILS_H
#define PERGYRA_TRANSPILER_MIR_SSA_UTILS_H

#include "transpiler.h"

bool transpiler_name_is_token_local(const char *name);
bool transpiler_type_name_is_slot_like(const char *type_name);
bool transpiler_type_name_is_view_like(const char *type_name);
bool transpiler_type_name_is_claim_shape(const char *type_name);
bool transpiler_block_has_claim_for_slot_local(const MIRBasicBlock *block,
                                               const char *slot_name);
bool transpiler_mir_routine_has_explicit_cfg(const MIRRoutine *routine);
bool transpiler_versioned_name_list_contains(const char **names,
                                             size_t count,
                                             const char *name);
bool transpiler_versioned_name_list_add(const char **names,
                                        size_t *count,
                                        size_t capacity,
                                        const char *name);
bool transpiler_c_type_uses_scalar_zero(const char *c_type);

#endif /* PERGYRA_TRANSPILER_MIR_SSA_UTILS_H */
