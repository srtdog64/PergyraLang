/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * capability_analyze.h -- capability manifest presentation.
 *
 * Capability inference, interprocedural propagation, and the per-function
 * `with caps` declared-vs-used check all live in the type checker now
 * (type_checker_func_decl.c + semantic_record_capability), where they reuse the
 * same sound, interprocedural machinery that enforces effects. The program's
 * inferred capability set is surfaced on SemanticResult.program_capabilities.
 *
 * This module is only the *presentation* of that set: it renders a capability
 * mask as the stable `pgy.capability.manifest.v1` JSON document a host reads to
 * decide what to grant (`pgy --capability-manifest`).
 *
 * SOUNDNESS NOTE (honest): the inferred set is a best-effort lower bound w.r.t.
 * dynamic dispatch / FFI (a static call graph cannot be complete -- Rice's
 * theorem). The runtime capability gate (pgy_cap_require_export) is the ground
 * truth and fail-closes, so any static under-count is safe, never permissive.
 */
#ifndef PERGYRA_CAPABILITY_ANALYZE_H
#define PERGYRA_CAPABILITY_ANALYZE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/*
 * Print a capability mask (OR of PGY_CAP_* bits) as the manifest JSON document.
 */
void capability_manifest_print(uint32_t used_mask, FILE *out);

/* Emit the used-capability names as a bare JSON array: ["IO_READ", "RANDOM"].
   Reused by AIR JSON so the effect-inventory fact is owned in one place. */
void capability_used_names_print_json(uint32_t used_mask, FILE *out);

/* The capability a gated ambient builtin requires (PGY_CAP_NONE if not gated).
   The single source of truth for per-operation effect->capability binding. */
uint32_t capability_for_builtin(const char *name);

/* The name of a single PGY_CAP_* bit, or NULL. */
const char *capability_bit_name(uint32_t bit);

/* Stable lower-case rendering used by semantic diagnostics. */
void capability_mask_to_diagnostic_string(uint32_t mask,
                                          char *buf,
                                          size_t buf_size);

#endif /* PERGYRA_CAPABILITY_ANALYZE_H */
