/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend match binding emission declarations.
 */

#ifndef PERGYRA_TRANSPILER_MATCH_BINDINGS_H
#define PERGYRA_TRANSPILER_MATCH_BINDINGS_H

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"
#include "transpiler.h"

bool transpiler_match_is_result_destructor(ASTNode *pat,
                                           const char **kind,
                                           const char **binding);
bool transpiler_match_is_option_destructor(ASTNode *pat,
                                           const char **kind,
                                           const char **binding);
bool transpiler_match_is_enum_variant_destructor(
    ASTNode *pat,
    TranspilerCtx *ctx,
    const char **variant_name_out,
    const char **enum_name_out,
    const char ***bindings_out,
    ASTNode ***binding_types_out,
    size_t *binding_count_out,
    const char **bindings_buf,
    ASTNode **binding_types_buf,
    size_t binding_cap);

void transpiler_emit_builtin_match_binding(ASTNode *pattern_node,
                                           const char *kind,
                                           const char *binding,
                                           const char *subject_type,
                                           bool subject_is_option,
                                           int tmp_id,
                                           TranspilerCtx *ctx);
void transpiler_emit_enum_match_bindings(ASTNode *pattern_node,
                                         const char *kind,
                                         int tmp_id,
                                         TranspilerCtx *ctx);

#endif /* PERGYRA_TRANSPILER_MATCH_BINDINGS_H */
