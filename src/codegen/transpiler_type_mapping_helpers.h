/* -----------------------------------------------------------------
 * Type mapping
 * ----------------------------------------------------------------- */

#ifndef PERGYRA_TRANSPILER_TYPE_MAPPING_HELPERS_H
#define PERGYRA_TRANSPILER_TYPE_MAPPING_HELPERS_H

#include <string.h>

#include "transpiler.h"

const char *pergyra_primitive_to_c(const char *name);
const char *slot_inner_type_name(const char *slot_type_name);
void sanitize_c_suffix(const char *type_name, char *buf, size_t buf_size);
void copy_capped_string(char *dst, size_t dst_size, const char *src);
const char *constructed_arg_name_at(const char *type_name, int arg_index);
void copy_constructed_arg_name_at(const char *type_name, int arg_index,
                                  char *buf, size_t buf_size);
const char *generic_args_to_c_suffix(const char *inner_body);

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
