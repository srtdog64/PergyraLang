/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy_runtime_cext_lib.c -- C-leg extern runtime object (inline->extern
 * workstream, docs/189 C14 / WO-RED2).
 *
 * One external-linkage definition of every converted runtime body, linked by
 * emitted C compiled in extern mode (PGY_RUNTIME_DECLS_ONLY). Built by
 * including the SAME *_inline.h headers the emitted C would otherwise inline,
 * under PGY_RUNTIME_EXTERN_DEFS so PGY_RT_DECL expands to an external-linkage
 * definition. Because object and emitted call site are cut from one signature,
 * the linked runtime is the exact twin of the historical inline body -- no
 * drift, no export gap.
 *
 * Families not yet converted stay `static inline` here; they compile into this
 * TU as unused internal symbols (harmless) and remain self-contained in the
 * emitted C. Only converted families cross to extern.
 */

#define PGY_RUNTIME_EXTERN_DEFS

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef PGY_INTENT_OBSERVABILITY_ENABLED
#define PGY_INTENT_OBSERVABILITY_ENABLED 0
#endif

#include "pgy_runtime.h"
#include "pgy_parallel.h"
#include "pgy_lane_scheduler.h"
#include "pgy_channel.h"
