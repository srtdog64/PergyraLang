#ifndef PGY_SOURCE_AST_FALLBACK_PROBE_H
#define PGY_SOURCE_AST_FALLBACK_PROBE_H

/*
 * Opt-in fire probe for the backend source_ast fallback arms.
 *
 * The slot and inventory views resolve declaration data MIR-first and only
 * reach for source_ast when MIR metadata is absent. The driver aborts when
 * MIR lowering fails, so codegen never runs without MIR and those fallback
 * arms are expected to be dead. This probe converts that expectation into
 * evidence: build with -DPGY_PROBE_SOURCE_AST_FALLBACK, run the corpus, and
 * grep stderr for the marker. Zero markers confirms the fallbacks never fire
 * and the source_ast readers can be retired.
 *
 * The probe is compiled out entirely unless the flag is set, so the release
 * build carries no instrumentation.
 */

#if defined(PGY_PROBE_SOURCE_AST_FALLBACK)

#include <stdio.h>

#define PGY_SOURCE_AST_FALLBACK_FIRE(site) \
    fprintf(stderr, "[source-ast-fallback] %s\n", (site))

#else

#define PGY_SOURCE_AST_FALLBACK_FIRE(site) ((void)0)

#endif /* PGY_PROBE_SOURCE_AST_FALLBACK */

#endif /* PGY_SOURCE_AST_FALLBACK_PROBE_H */
