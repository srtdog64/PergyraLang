#ifndef PGY_TRANSPILER_DOMAIN_RECEIVER_QUERY_H
#define PGY_TRANSPILER_DOMAIN_RECEIVER_QUERY_H

#include <stdbool.h>

#include "transpiler.h"

ASTNode *transpiler_find_subject_host_method_decl(TranspilerCtx *ctx,
                                                  const char *type_name,
                                                  const char *method_name);
bool transpiler_resolve_zone_subject_receiver(TranspilerCtx *ctx,
                                              ASTNode *receiver,
                                              const char **slot_name_out,
                                              const char **type_name_out);
bool transpiler_resolve_world_zone_subject_receiver(
    TranspilerCtx *ctx,
    ASTNode *receiver,
    const char **zone_slot_name_out,
    const char **zone_type_name_out,
    const char **slot_name_out,
    const char **type_name_out);

#endif /* PGY_TRANSPILER_DOMAIN_RECEIVER_QUERY_H */
