/*
 * Copyright (c) 2026 Pergyra Language Project
 * Post-fan-out half of the C join emitter (docs/182 SS5): the join walk
 * (including the R3 any-join decision wait) and the expression-form
 * result materialization (R2 index-order collection, R4 reduce fold).
 * The fan-out half (captures, context struct, wrapper) stays in
 * transpiler_parallel_join_emit.c.
 */

#ifndef PERGYRA_TRANSPILER_PARALLEL_JOIN_REDUCE_EMIT_H
#define PERGYRA_TRANSPILER_PARALLEL_JOIN_REDUCE_EMIT_H

#include "transpiler.h"
#include "../parser/ast.h"

/* Await every handle exactly once. Any-mode prepends the R3 slice-2
 * decision spin + cancel-all and appends the empty-fan-out panic. */
void transpiler_pjoin_emit_join_await(ASTNode *node, TranspilerCtx *ctx,
                                      const char *give_suffix,
                                      unsigned int pid);

/* Expression-form results after the join: index-order pgy_array_push
 * collection, or the R4 fixed left fold (checked-arith lanes for
 * Int/Long sum/product, seeded min/max with the empty panic). No-op in
 * statement form and any mode (any materializes through the shared
 * winner cell instead). */
void transpiler_pjoin_emit_result_materialize(ASTNode *node,
                                              TranspilerCtx *ctx,
                                              const char *give_suffix,
                                              const char *give_c_type,
                                              const char *res_name,
                                              unsigned int pid);

#endif /* PERGYRA_TRANSPILER_PARALLEL_JOIN_REDUCE_EMIT_H */
