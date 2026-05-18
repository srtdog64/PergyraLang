/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared support for C backend collection stdlib lowering.
 */

#include "transpiler_expr_stdlib_collection_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_enum.h"
#include "transpiler_format.h"
#include "transpiler_nominal.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

static bool
transpiler_collection_copy_spec_name(char *dst, size_t dst_size,
                                     const char *value)
{
    size_t len;

    if (dst == NULL || dst_size == 0 || value == NULL)
        return false;

    len = strlen(value);
    if (len >= dst_size)
        return false;

    memcpy(dst, value, len + 1);
    return true;
}

static void
transpiler_collection_ensure_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                               const char *kind,
                                               const char *inner_type)
{
    char suffix[128];
    char ctype_buf[128];

    if (ctx == NULL || dst == NULL || kind == NULL || inner_type == NULL)
        return;

    if ((strcmp(kind, "List") == 0 || strcmp(kind, "Queue") == 0)
        && (strcmp(inner_type, "Int") == 0
            || strcmp(inner_type, "String") == 0)) {
        return;
    }
    if ((strcmp(kind, "Map") == 0 || strcmp(kind, "Set") == 0)
        && (strcmp(inner_type, "Int") == 0
            || strcmp(inner_type, "String") == 0)) {
        return;
    }

    sanitize_c_suffix(inner_type, suffix, sizeof(suffix));
    for (int i = 0; i < ctx->collection_spec_count; i++) {
        if (strcmp(ctx->collection_specs[i].kind, kind) == 0
            && strcmp(ctx->collection_specs[i].suffix, suffix) == 0) {
            return;
        }
    }

    if (ctx->collection_spec_count >= MAX_COLLECTION_SPECIALIZATIONS)
        return;
    if (!pergyra_type_to_c_copy(inner_type, ctype_buf, sizeof(ctype_buf)))
        return;
    if (!transpiler_collection_copy_spec_name(
            ctx->collection_specs[ctx->collection_spec_count].kind,
            sizeof(ctx->collection_specs[ctx->collection_spec_count].kind),
            kind)
        || !transpiler_collection_copy_spec_name(
            ctx->collection_specs[ctx->collection_spec_count].suffix,
            sizeof(ctx->collection_specs[ctx->collection_spec_count].suffix),
            suffix)) {
        return;
    }
    ctx->collection_spec_count++;

    if (strcmp(kind, "List") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_LIST_%s */\n"
            "PGY_LIST_DEFINE(%s, %s)\n",
            suffix, suffix, ctype_buf);
    } else if (strcmp(kind, "Queue") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_QUEUE_%s */\n"
            "PGY_QUEUE_DEFINE(%s, %s)\n",
            suffix, suffix, ctype_buf);
    } else if (strcmp(kind, "Map") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_MAP_%s */\n"
            "PGY_HASHMAP_DEFINE(%s, %s)\n",
            suffix, suffix, ctype_buf);
    } else if (strcmp(kind, "Set") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_SET_%s */\n"
            "PGY_SET_DEFINE(%s, %s)\n",
            suffix, suffix, ctype_buf);
    }
}

void
transpiler_collection_ensure_specialization(TranspilerCtx *ctx,
                                            const char *kind,
                                            const char *inner_type)
{
    transpiler_collection_ensure_specialization_to(ctx,
        ctx != NULL ? ctx->decls : NULL, kind, inner_type);
}
