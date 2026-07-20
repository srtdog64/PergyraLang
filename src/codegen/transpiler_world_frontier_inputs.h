#ifndef PGY_TRANSPILER_WORLD_FRONTIER_INPUTS_H
#define PGY_TRANSPILER_WORLD_FRONTIER_INPUTS_H

#include <stddef.h>

size_t transpiler_frontier_zone_member_count(void *ctx,
                                             const char *zone_name);
const char *transpiler_frontier_world_zone_type_name(void *ctx,
                                                     size_t index);

#endif /* PGY_TRANSPILER_WORLD_FRONTIER_INPUTS_H */
