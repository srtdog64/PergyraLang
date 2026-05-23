/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Whole-program intent observability usage query.
 *
 * Codegen needs this before emitting declarations. Lowered MIR should carry
 * inventory surface facts; compatibility scans remain only for hand-built MIR
 * fixtures that do not have HIR provenance.
 */

#ifndef PERGYRA_INTENT_OBSERVABILITY_USAGE_H
#define PERGYRA_INTENT_OBSERVABILITY_USAGE_H

#include <stdbool.h>

typedef struct MIRProgram MIRProgram;

bool pgy_mir_program_uses_intent_observability(const MIRProgram *mir);

#endif /* PERGYRA_INTENT_OBSERVABILITY_USAGE_H */
