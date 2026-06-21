/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * Domain-lifecycle analysis pass - see lifecycle_analyze.h and
 * docs/semantics/12_domain_lifecycle_evidence.md.
 */

#include "lifecycle_analyze.h"
#include "lifecycle_state.h"
#include "../parser/ast_api.h"

/*
 * Collect lifecycle declarations from the program into machines. The
 * declaration surface (a `lifecycle <Subject> { states ...; Op: A -> B; }`
 * top-level form) is not parsed yet, so today this finds nothing and the pass
 * is a clean no-op. The hook is here so the surface + value-tracking walk plug
 * in at exactly one place without re-plumbing the pipeline.
 *
 * When the surface lands, this becomes:
 *   1. for each lifecycle decl -> build an LcMachine (states + transitions);
 *   2. for each function body -> walk statements tracking each governed local's
 *      LcState: construction -> initial; `v.Op()` -> lc_apply_op (emit a static
 *      error on LC_ERR_PRECONDITION, mark a runtime state-tag check on
 *      LC_NEEDS_RUNTIME_CHECK); branch joins -> lc_merge.
 */
static int
lifecycle_collect_machines(ASTNode *program)
{
    int machine_count = 0;

    if (program == NULL)
        return 0;
    /* Surface-integration point: when AST carries lifecycle declarations,
     * iterate them here and build one LcMachine each. No surface node type
     * exists yet, so nothing is collected and no program is affected. */
    return machine_count;
}

bool
lifecycle_analyze_program(ASTNode *program, SemanticContext *ctx)
{
    (void)ctx;
    if (program == NULL)
        return true;

    /* No lifecycle declarations -> nothing to enforce. This guarantees the pass
     * never produces a false positive on existing (lifecycle-free) programs. */
    if (lifecycle_collect_machines(program) == 0)
        return true;

    /* Reached only once the declaration surface + value-tracking walk land. */
    return true;
}
