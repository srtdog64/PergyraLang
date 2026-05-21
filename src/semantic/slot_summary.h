/*
 * Copyright (c) 2025 Pergyra Language Project
 * Slot escape/parameter summary API shared by semantic analysis and codegen.
 */

#ifndef PERGYRA_SLOT_SUMMARY_H
#define PERGYRA_SLOT_SUMMARY_H

#include "../parser/ast.h"

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
    SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE = 1 << 5
} SlotParamSummaryFlags;

unsigned slot_analyze_escape_flags(ASTNode *node, const char *slot_name);
unsigned slot_analyze_escape_flags_in_program(ASTNode *node,
                                              const char *slot_name,
                                              ASTNode *program_root);
/*
 * Compatibility seam: AST-walking parameter summaries are retained for
 * diagnostic/provenance coverage while CFG/MIR body facts are being promoted
 * to the beta-final source of truth. New safety consumers should prefer
 * CFG/MIR facts; use this name only when the AST compatibility path is
 * intentionally accepted and smoke-gated.
 */
unsigned slot_analyze_legacy_ast_param_summary_in_program(ASTNode *node,
                                                          const char *slot_name,
                                                          ASTNode *program_root);

#endif /* PERGYRA_SLOT_SUMMARY_H */
