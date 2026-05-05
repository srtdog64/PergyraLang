/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR let-declaration lookup helpers.
 */

#ifndef PERGYRA_TRANSPILER_MIR_LET_LOOKUP_H
#define PERGYRA_TRANSPILER_MIR_LET_LOOKUP_H

#include "transpiler.h"

ASTNode *transpiler_find_let_decl_by_name(const ASTNode *func_decl,
                                          const char *name);

#endif /* PERGYRA_TRANSPILER_MIR_LET_LOOKUP_H */
