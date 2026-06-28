/*
 * Copyright (c) 2025 Pergyra Language Project
 *
 * for-in non-identifier iterable desugar (post-parse, pre-semantic).
 * See forin_desugar.c for the full rationale.
 */

#ifndef PERGYRA_FORIN_DESUGAR_H
#define PERGYRA_FORIN_DESUGAR_H

#include "../parser/ast_types.h"

/* Rewrite every `for VAR in EXPR { BODY }` whose EXPR is non-NULL and not a
 * bare identifier into `{ let __pgy_forin_N = EXPR; for VAR in __pgy_forin_N
 * { BODY } }`, hoisting the iterable into a synthetic local evaluated once.
 * Range loops and identifier/variable iterables are left untouched. Mutates
 * the program AST in place. Must run in the compile path only (after the
 * `--ast` dump, before semantic analysis) so parser-parity is preserved. */
void forin_desugar_program(ASTNode *program);

#endif /* PERGYRA_FORIN_DESUGAR_H */
