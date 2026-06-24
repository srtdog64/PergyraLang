#ifndef PERGYRA_SQUIGGLE_CLASS_H
#define PERGYRA_SQUIGGLE_CLASS_H

#include <stdbool.h>

#include "diagnostic_layer.h"

/*
 * Semantic squiggle class (docs/140).
 *
 * The editor-facing colour of a diagnostic. This is NOT severity: it encodes
 * *which dimension of meaning* the diagnostic is about, orthogonally to whether
 * the diagnostic blocks compilation. Only RED blocks; the others are advisory
 * squiggles drawn without failing the build.
 *
 *   RED    - blocking syntax/type error
 *   AMBER  - axis mismatch (Intent / Role / Subject / Vessel / lifecycle)
 *   VIOLET - authority/world boundary (World / Zone / capability / pin / effect)
 *   BLUE   - meaning erased at runtime (AIR erasure) -- reserved; the live
 *            diagnostic path is not yet wired to AIR erasure data (docs/14)
 *   NONE   - no semantic squiggle (plain warning / uncategorised)
 *
 * This is a pure classification policy: one lookup, no I/O, no allocation.
 */
typedef enum
{
    SQUIGGLE_NONE,
    SQUIGGLE_RED,
    SQUIGGLE_AMBER,
    SQUIGGLE_VIOLET,
    SQUIGGLE_BLUE
} SquiggleClass;

/* Stable lowercase name for JSON/LSP payloads ("red"/"amber"/...). */
const char *squiggle_class_name(SquiggleClass cls);

/*
 * Classify a diagnostic into a squiggle class.
 *
 *   is_blocking - true when the diagnostic fails compilation (level == error).
 *                 A blocking diagnostic is always RED regardless of dimension
 *                 (fail-closed: what must be blocked is shown as blocking).
 *   layer       - the DiagnosticLayer (used as a fallback when the code is
 *                 uncoded/ambiguous).
 *   code        - the stable PGY_CODE_* identifier (primary signal; NULL ok).
 */
SquiggleClass squiggle_class_classify(bool is_blocking,
                                      DiagnosticLayer layer,
                                      const char *code);

#endif /* PERGYRA_SQUIGGLE_CLASS_H */
