/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent participant classification helpers.
 */

#ifndef PERGYRA_TRANSPILER_INTENT_PARTICIPANT_H
#define PERGYRA_TRANSPILER_INTENT_PARTICIPANT_H

#include "transpiler.h"

const char *intent_involves_type_name_local(ASTNode *involves);
bool intent_involves_is_subject_participant(TranspilerCtx *ctx,
                                            ASTNode *involves);
bool intent_involves_uses_pointer_self(TranspilerCtx *ctx,
                                       ASTNode *involves);

#endif /* PERGYRA_TRANSPILER_INTENT_PARTICIPANT_H */
