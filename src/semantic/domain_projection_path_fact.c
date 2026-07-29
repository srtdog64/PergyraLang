#include "domain_projection_path_fact_internal.h"
#include "parser/ast_api.h"

#include <stdlib.h>

void
pgy_domain_projection_path_segments_destroy(
    PgyDomainProjectionPathSegmentFact *segments,
    size_t segment_count)
{
    for (size_t i = 0; segments != NULL && i < segment_count; i++) {
        free(segments[i].field_name);
        free(segments[i].field_type_name);
    }
    free(segments);
}

PgyDomainProjectionOperation
domain_projection_operation(ASTNode *directive)
{
    if (directive != NULL && directive->type == AST_ZONE_REFRESH) {
        if (ast_zone_refresh_derives_target_kind(directive))
            return PGY_DOMAIN_PROJECTION_BIND;
        if (ast_zone_refresh_requires_dto(directive))
            return PGY_DOMAIN_PROJECTION_PUBLISH;
    }
    return PGY_DOMAIN_PROJECTION_REFRESH;
}
