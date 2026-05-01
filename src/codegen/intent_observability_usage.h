/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Whole-program intent observability usage scan.
 *
 * Codegen needs this before emitting declarations: intent declarations may
 * appear before a later routine that calls IntentLast/IntentHistory builtins.
 * Builtin emission is therefore too late to decide whether trace/history
 * bookkeeping should be generated.
 */

#ifndef PERGYRA_INTENT_OBSERVABILITY_USAGE_H
#define PERGYRA_INTENT_OBSERVABILITY_USAGE_H

#include <stdbool.h>

typedef struct MIRProgram MIRProgram;

bool pgy_mir_program_uses_intent_observability(const MIRProgram *mir);

#endif /* PERGYRA_INTENT_OBSERVABILITY_USAGE_H */
