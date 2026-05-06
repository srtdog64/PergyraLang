/* -----------------------------------------------------------------
 * Type mapping
 * ----------------------------------------------------------------- */

#ifndef PERGYRA_TRANSPILER_TYPE_MAPPING_HELPERS_H
#define PERGYRA_TRANSPILER_TYPE_MAPPING_HELPERS_H

#include <string.h>

#include "transpiler.h"
#include "transpiler_type_mapping.h"

static void ensure_result_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                            const char *ok_type,
                                            const char *err_type);

static const char *
channel_inner_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    const char *type_name = infer_expression_type_name(ctx, expr);
    if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
        return slot_inner_type_name(type_name);
    return "Unknown";
}

#include "transpiler_type_result_mapping_helpers.h"
#include "transpiler_type_render_helpers.h"

#endif /* PERGYRA_TRANSPILER_TYPE_MAPPING_HELPERS_H */
