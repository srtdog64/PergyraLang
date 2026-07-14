#include "mir_timing.h"

#include <stdio.h>
#include <time.h>

/* PGY_DEBUG_MIR_TIMING=1 prints where mir_lower's wallclock lives, one
 * accumulated line per sub-stage. The timing owner remains observational:
 * lowering records slots but never reads the backing storage directly. */
static const char *const kMirTimingNames[MIR_TIMING_SLOT_COUNT] = {
    "rir_scope_match", "signature_capture", "source_local_types",
    "build_blocks", "cleanup_block", "populate_instructions",
    "ssa_rename", "stmt_instructions", "speculation_facts",
    "use_edges", "cleanup_edges", "recompute_analysis", "dce_pass",
    "  bb/source_location", "  bb/copies", "  bb/append",
    "  bb/phi_terminator",
};

typedef struct
{
    double slots[MIR_TIMING_SLOT_COUNT];
} MIRTimingState;

static MIRTimingState *
mir_timing_state(void)
{
    static _Thread_local MIRTimingState state;
    return &state;
}

double
mir_timing_now(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

void
mir_timing_add(MIRTimingSlot slot, double elapsed_seconds)
{
    if (slot < MIR_TIMING_RIR_MATCH || slot >= MIR_TIMING_SLOT_COUNT)
        return;
    mir_timing_state()->slots[slot] += elapsed_seconds;
}

double
mir_timing_total(void)
{
    double total = 0.0;
    for (int i = 0; i < MIR_TIMING_SLOT_COUNT; i++)
        total += mir_timing_state()->slots[i];
    return total;
}

void
mir_timing_report(void)
{
    fprintf(stderr, "[mir timing] sub-stage totals:\n");
    for (int i = 0; i < MIR_TIMING_SLOT_COUNT; i++) {
        fprintf(stderr, "[mir timing]   %-22s %8.3fs\n",
                kMirTimingNames[i], mir_timing_state()->slots[i]);
    }
}
