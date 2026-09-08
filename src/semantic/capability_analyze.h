/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * capability_analyze.h -- capability manifest presentation.
 *
 * Capability inference, interprocedural propagation, and the per-function
 * `with caps` declared-vs-used check all live in the type checker now
 * (`callable_capability_inference.c` + builtin capability recording). The
 * program seal substitutes invoked callable identities before checking bounds;
 * a function-type mask alone cannot describe a callable formal's actual use.
 * The inferred set is surfaced on SemanticResult.program_capabilities.
 *
 * This module is only the *presentation* of that set: it renders a capability
 * mask as the stable `pgy.capability.manifest.v1` JSON document a host reads to
 * decide what to grant (`pgy --capability-manifest`).
 *
 * Open callable templates retain deferred use. Unresolved provenance at a
 * closed invocation is an admission error, not a zero mask. This does not prove
 * complete FFI/dynamic-dispatch coverage: foreign declarations require their
 * explicit contract and runtime enforcement remains an independent boundary.
 * A runtime check does not repair a missing static capability rejection.
 */
#ifndef PERGYRA_CAPABILITY_ANALYZE_H
#define PERGYRA_CAPABILITY_ANALYZE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

/*
 * Print a capability mask (OR of PGY_CAP_* bits) as the manifest JSON document.
 */
void capability_manifest_print(uint32_t used_mask, FILE *out);

/* Emit the used-capability names as a bare JSON array: ["IO_READ", "RANDOM"].
   Reused by AIR JSON so the effect-inventory fact is owned in one place. */
void capability_used_names_print_json(uint32_t used_mask, FILE *out);

/* The capability mask a name-only ambient builtin requires (PGY_CAP_NONE if
   not gated). Mode-sensitive FileOpen is refined from its AST operand by the
   semantic checker and enforced from the concrete mode by the runtime; AIR
   cannot claim an exact FileOpen site until MIR carries that mode fact. */
uint32_t capability_for_builtin(const char *name);

/* The generated builtin-policy registry is internally consistent with the
   callable capability vocabulary and its canonical mask projection. */
bool capability_builtin_registry_ready(void);

/* The name of a single PGY_CAP_* bit, or NULL. */
const char *capability_bit_name(uint32_t bit);

/* Stable lower-case rendering used by semantic diagnostics. */
void capability_mask_to_diagnostic_string(uint32_t mask,
                                          char *buf,
                                          size_t buf_size);

#endif /* PERGYRA_CAPABILITY_ANALYZE_H */
