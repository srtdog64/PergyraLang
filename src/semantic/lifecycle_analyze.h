/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * Domain-Lifecycle Analysis Pass.
 *
 * Drives the N-state lifecycle engine (lifecycle_state.{h,c}) over a program:
 * collects lifecycle declarations, builds an LcMachine per stateful subject, and
 * tracks each governed value's state through a function body -- static-rejecting
 * an operation whose precondition the current (statically-known) state violates,
 * and flagging the ambiguous case for a fail-closed runtime state-tag check.
 *
 * See docs/semantics/12_domain_lifecycle_evidence.md.
 *
 * STATUS: fully wired across all three layers. The engine + semantic pass
 * consume parser-owned AST_LIFECYCLE_DECL nodes (layers 1-2: static reject where
 * provable). Ambiguous control-flow joins are annotated onto the call/let AST
 * nodes and lowered by BOTH backends to a fail-closed runtime state-tag guard
 * (layer 3: pgy_runtime_lifecycle_guard_export, panic class
 * "invalid-lifecycle-state"). Lifecycle-free programs remain a clean no-op and
 * fully-provable variables stay zero-cost (taint keeps runtime instrumentation
 * to variables that actually reach an ambiguous op).
 */

#ifndef PERGYRA_LIFECYCLE_ANALYZE_H
#define PERGYRA_LIFECYCLE_ANALYZE_H

#include <stdbool.h>
#include "../parser/ast.h"

typedef struct SemanticContext SemanticContext;

/*
 * Run the lifecycle analysis over the whole program. Returns true on success
 * (including the no-machines no-op case); emits semantic diagnostics through
 * ctx on a precondition violation. Never rejects a program that declares no
 * lifecycles.
 */
bool lifecycle_analyze_program(ASTNode *program, SemanticContext *ctx);

#endif /* PERGYRA_LIFECYCLE_ANALYZE_H */
