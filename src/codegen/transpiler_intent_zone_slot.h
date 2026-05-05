/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent zone-slot resolution helpers.
 */

#ifndef PERGYRA_TRANSPILER_INTENT_ZONE_SLOT_H
#define PERGYRA_TRANSPILER_INTENT_ZONE_SLOT_H

#include "transpiler.h"

const char *resolve_intent_zone_slot_name(TranspilerCtx *ctx, ASTNode *intent,
                                          ASTNode *step, const char *alias);
const char *resolve_intent_zone_slot_name_for_zone(TranspilerCtx *ctx,
                                                   ASTNode *intent,
                                                   const char *zone_type_name,
                                                   const char *alias);

#endif /* PERGYRA_TRANSPILER_INTENT_ZONE_SLOT_H */
