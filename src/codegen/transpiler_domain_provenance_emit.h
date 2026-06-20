#ifndef PGY_TRANSPILER_DOMAIN_PROVENANCE_EMIT_H
#define PGY_TRANSPILER_DOMAIN_PROVENANCE_EMIT_H

#include <stdbool.h>
#include <stddef.h>

#include "domain_frontier_policy.h"
#include "transpiler.h"
#include "transpiler_decl_lookup.h"

enum {
    PGY_PROP_CAUSE_NONE = 0,
    PGY_PROP_CAUSE_REFRESH = 1,
    PGY_PROP_CAUSE_APPLY = 2,
    PGY_PROP_CAUSE_MAINTAIN = 3,
    PGY_PROP_CAUSE_DETACH = 4,
    PGY_PROP_CAUSE_LINK = 5,
    PGY_PROP_CAUSE_UNLINK = 6,
    PGY_PROP_CAUSE_WORLD_ACTIVATE = 7,
    PGY_PROP_CAUSE_WORLD_MAINTAIN = 8,
    PGY_PROP_CAUSE_WORLD_DEACTIVATE = 9,
    PGY_PROP_CAUSE_WORLD_DERIVED = 10,
    PGY_PROP_CAUSE_ACTION = 11,
};

void emit_hidden_provenance_fields(TranspilerCtx *ctx,
                                   const char *prefix,
                                   const char *name);
void emit_hidden_provenance_stamp(TranspilerCtx *ctx,
                                  const char *self_expr,
                                  const char *prefix,
                                  const char *name,
                                  int cause);
void emit_zone_projection_sync_loop_from_mir_refresh_view(
    TranspilerCtx *ctx,
    const TranspilerHostedDomainSlotView *slot_view,
    const TranspilerHostedZoneRefreshView *refresh_view,
    const char *loop_prefix,
    bool early_return_if_clean);

#endif /* PGY_TRANSPILER_DOMAIN_PROVENANCE_EMIT_H */
