/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend zone frontier guard emission declarations.
 */

#ifndef PERGYRA_TRANSPILER_ZONE_FRONTIER_EMIT_H
#define PERGYRA_TRANSPILER_ZONE_FRONTIER_EMIT_H

#include <stddef.h>

#include "../parser/ast.h"
#include "transpiler.h"

void transpiler_emit_zone_frontier_change_checks(TranspilerCtx *ctx,
                                                 ASTNode **states,
                                                 size_t state_count,
                                                 ASTNode **layer_slots,
                                                 size_t layer_slot_count);
void transpiler_emit_zone_frontier_overflow_guard(TranspilerCtx *ctx);

#endif /* PERGYRA_TRANSPILER_ZONE_FRONTIER_EMIT_H */
