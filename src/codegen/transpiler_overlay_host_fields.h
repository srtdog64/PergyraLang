#ifndef PGY_TRANSPILER_OVERLAY_HOST_FIELDS_H
#define PGY_TRANSPILER_OVERLAY_HOST_FIELDS_H

#include <stdbool.h>

#include "transpiler.h"

bool current_class_uses_self_cell(TranspilerCtx *ctx);
bool current_class_has_field(TranspilerCtx *ctx, const char *field_name);
bool current_zone_has_field(TranspilerCtx *ctx, const char *field_name);
bool current_party_has_field(TranspilerCtx *ctx, const char *field_name);
bool current_roster_has_field(TranspilerCtx *ctx, const char *field_name);
bool current_relation_has_field(TranspilerCtx *ctx, const char *field_name);
bool current_effect_has_field(TranspilerCtx *ctx, const char *field_name);

#endif /* PGY_TRANSPILER_OVERLAY_HOST_FIELDS_H */
