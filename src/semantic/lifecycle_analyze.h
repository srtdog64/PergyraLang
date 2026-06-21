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
 * STATUS: the engine + this pass are wired into the semantic pipeline and the
 * build. The user-facing surface that declares states/transitions, and the
 * value-tracking AST walk + runtime-tag codegen, are the remaining layers; until
 * a lifecycle declaration surface exists this pass collects zero machines and is
 * a clean no-op (no false positives on existing programs).
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
