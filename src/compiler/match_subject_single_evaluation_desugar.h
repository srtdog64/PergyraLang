/*
 * Copyright (c) 2026 Pergyra Language Project
 *
 * Match-subject single-evaluation normalization (post-parse, pre-semantic).
 */

#ifndef PERGYRA_MATCH_SUBJECT_SINGLE_EVALUATION_DESUGAR_H
#define PERGYRA_MATCH_SUBJECT_SINGLE_EVALUATION_DESUGAR_H

#include "../parser/ast_types.h"

/* Hoist every non-trivial `match EXPR` subject into one synthetic local so
 * semantic/MIR consumers and every backend observe one evaluation. */
void match_subject_single_evaluation_desugar_program(ASTNode *program);

#endif
