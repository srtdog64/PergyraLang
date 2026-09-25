/*
 * Copyright (c) 2025 Pergyra Language Project
 * Slot escape/parameter summary API shared by semantic analysis and codegen.
 */

#ifndef PERGYRA_SLOT_SUMMARY_H
#define PERGYRA_SLOT_SUMMARY_H

#include "../parser/ast.h"

typedef struct SemanticContext SemanticContext;

typedef enum
{
    SLOT_ESCAPE_NONE    = 0,
    SLOT_ESCAPE_RETURN  = 1 << 0,
    SLOT_ESCAPE_CALL    = 1 << 1,
    SLOT_ESCAPE_CHANNEL = 1 << 2
} SlotEscapeFlags;

typedef enum
{
    SLOT_PARAM_SUMMARY_NONE           = 0,
    SLOT_PARAM_SUMMARY_READ           = 1 << 0,
    SLOT_PARAM_SUMMARY_WRITE          = 1 << 1,
    SLOT_PARAM_SUMMARY_RELEASE        = 1 << 2,
    SLOT_PARAM_SUMMARY_RETURN_ESCAPE  = 1 << 3,
    SLOT_PARAM_SUMMARY_CALL_ESCAPE    = 1 << 4,
    SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE = 1 << 5,
    SLOT_PARAM_SUMMARY_ALL            = (1 << 6) - 1
} SlotParamSummaryFlags;

/*
 * The demanded parameter flow summary owned by function_param_flow_summary.c.
 * Without a semantic context there is no owner to ask, so the answer is
 * SLOT_PARAM_SUMMARY_ALL; no caller walks the callee body itself.
 */
unsigned function_param_flow_summary_for_param(SemanticContext *ctx,
                                               ASTNode *function_decl,
                                               size_t param_index);

#endif /* PERGYRA_SLOT_SUMMARY_H */
