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

static bool
channel_inner_type_name_copy(TranspilerCtx *ctx, ASTNode *expr,
                             char *out, size_t out_size)
{
    const char *type_name = infer_expression_type_name(ctx, expr);

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
        return slot_inner_type_name_copy(type_name, out, out_size);
    return pergyra_str_copy(out, out_size, "Unknown");
}

#include "transpiler_type_result_mapping_helpers.h"
#include "transpiler_type_render_helpers.h"

#endif /* PERGYRA_TRANSPILER_TYPE_MAPPING_HELPERS_H */
