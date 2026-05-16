#ifndef PERGYRA_MIR_SURFACE_USAGE_H
#define PERGYRA_MIR_SURFACE_USAGE_H

#include <stdbool.h>

#include "mir.h"

typedef struct MIRSurfaceUsageSummary {
    bool uses_thread_pool;
    bool uses_intent_observability;
} MIRSurfaceUsageSummary;

MIRSurfaceUsageSummary mir_inventory_surface_usage_summary(
    const MIRProgram *mir);
bool mir_inventory_uses_thread_pool_surface(const MIRProgram *mir);
bool mir_inventory_uses_intent_observability_surface(const MIRProgram *mir);
void mir_program_record_inventory_surface_usage(MIRProgram *mir);

#endif /* PERGYRA_MIR_SURFACE_USAGE_H */
