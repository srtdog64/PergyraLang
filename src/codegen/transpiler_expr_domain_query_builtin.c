/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend domain query builtins.
 */

#include "transpiler_expr_domain_query_builtin.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "host_decl_compat.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_projection.h"

static char *
domain_query_heap_fmt(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;
    int n;
    char *s;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: domain query expression formatting failed");
        return NULL;
    }

    s = (char *)malloc((size_t)n + 1);
    if (s == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: domain query expression allocation failed");
        return NULL;
    }

    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

static const char *
domain_query_name_arg(ASTNode *arg)
{
    if (arg == NULL)
        return NULL;
    if (arg->type == AST_IDENTIFIER)
        return ast_identifier_name(arg);
    if (arg->type == AST_STRING)
        return ast_string_value(arg);
    return NULL;
}

static char *
domain_query_unsupported(TranspilerCtx *ctx, const char *message)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        message);
    return NULL;
}

static char *
emit_builtin_has_projection(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *arg0 = ast_call_argument(call, 0);
    const char *slot_name = domain_query_name_arg(arg0);

    if (ast_call_arg_count(call) == 1 && slot_name != NULL) {
        if (transpiler_current_overlay_domain_slot_is_projection(
                ctx, slot_name)) {
            return domain_query_heap_fmt(ctx, "self->__projection_ready_%s",
                slot_name);
        }
        {
            ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
            if (pgy_host_decl_compat_has_projection_ready_flag(host_decl)) {
                return domain_query_heap_fmt(ctx, "self->__projection_ready_%s",
                    slot_name);
            }
        }
    }

    return domain_query_unsupported(ctx,
        "C backend: HasProjection requires active relation/effect/zone projection context");
}

static char *
emit_builtin_has_layer(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *arg0 = ast_call_argument(call, 0);
    ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
    ASTNode *zone_decl = host_decl != NULL && host_decl->type == AST_ZONE_DECL
        ? host_decl
        : NULL;
    const char *layer_name = domain_query_name_arg(arg0);

    if (zone_decl != NULL
        && ast_call_arg_count(call) == 1
        && layer_name != NULL) {
        const char *zone_name = transpiler_decl_name_local(zone_decl);
        if (zone_name == NULL)
            return domain_query_unsupported(ctx,
                "C backend: HasLayer requires active zone context");
        return domain_query_heap_fmt(ctx,
            "%s_has_layer_%s(self, __pgy_zone_gen)",
            zone_name, layer_name);
    }

    return domain_query_unsupported(ctx,
        "C backend: HasLayer requires active zone context");
}

static char *
emit_builtin_has_state(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *arg0 = ast_call_argument(call, 0);
    ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
    ASTNode *zone_decl = host_decl != NULL && host_decl->type == AST_ZONE_DECL
        ? host_decl
        : NULL;
    const char *state_name = domain_query_name_arg(arg0);
    ASTNode *state_decl = zone_decl != NULL
        ? transpiler_find_zone_state_decl(zone_decl, state_name)
        : NULL;

    if (zone_decl != NULL
        && ast_call_arg_count(call) >= 1
        && state_decl != NULL
        && state_name != NULL) {
        return domain_query_heap_fmt(ctx, "self->__state_%s", state_name);
    }

    return domain_query_unsupported(ctx,
        "C backend: HasState requires active zone context with matching state");
}

static char *
emit_builtin_has_zone(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *arg0 = ast_call_argument(call, 0);
    ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
    ASTNode *world_decl = host_decl != NULL && host_decl->type == AST_WORLD_DECL
        ? host_decl
        : NULL;
    const char *name = domain_query_name_arg(arg0);
    ASTNode *state_decl = world_decl != NULL
        ? transpiler_find_world_state_decl(world_decl, name)
        : NULL;

    if (world_decl != NULL
        && ast_call_arg_count(call) >= 1
        && name != NULL) {
        if (state_decl != NULL)
            return domain_query_heap_fmt(ctx, "self->__zone_state_%s", name);
        if (transpiler_world_has_zone_slot(ctx, world_decl, name))
            return domain_query_heap_fmt(ctx, "self->__zone_active_%s", name);
    }

    return domain_query_unsupported(ctx,
        "C backend: HasZone requires active world context");
}

static char *
emit_builtin_has_zone_projection(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *arg0 = ast_call_argument(call, 0);
    ASTNode *arg1 = ast_call_argument(call, 1);
    ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
    ASTNode *world_decl = host_decl != NULL && host_decl->type == AST_WORLD_DECL
        ? host_decl
        : NULL;
    const char *zone_name = domain_query_name_arg(arg0);
    const char *slot_name = domain_query_name_arg(arg1);
    ASTNode *zone_decl = world_decl != NULL
        ? transpiler_resolve_world_zone_decl(ctx, world_decl, zone_name)
        : NULL;

    if (world_decl != NULL
        && ast_call_arg_count(call) == 2
        && zone_decl != NULL
        && transpiler_zone_domain_slot_is_projection(
            ctx, zone_decl, slot_name)) {
        return domain_query_heap_fmt(ctx, "self->%s.__projection_ready_%s",
            zone_name, slot_name);
    }

    return domain_query_unsupported(ctx,
        "C backend: HasZoneProjection requires active world context with matching zone projection");
}

static char *
emit_builtin_has_zone_layer(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *arg0 = ast_call_argument(call, 0);
    ASTNode *arg1 = ast_call_argument(call, 1);
    ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
    ASTNode *world_decl = host_decl != NULL && host_decl->type == AST_WORLD_DECL
        ? host_decl
        : NULL;
    const char *zone_name = domain_query_name_arg(arg0);
    const char *layer_name = domain_query_name_arg(arg1);
    ASTNode *zone_decl = world_decl != NULL
        ? transpiler_resolve_world_zone_decl(ctx, world_decl, zone_name)
        : NULL;

    if (world_decl != NULL
        && ast_call_arg_count(call) == 2
        && zone_decl != NULL
        && layer_name != NULL
        && transpiler_zone_has_layer_slot(ctx, zone_decl, layer_name)) {
        return domain_query_heap_fmt(ctx, "self->%s.__layer_active_%s",
            zone_name, layer_name);
    }

    return domain_query_unsupported(ctx,
        "C backend: HasZoneLayer requires active world context with matching zone layer");
}

static char *
emit_builtin_has_zone_state(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *arg0 = ast_call_argument(call, 0);
    ASTNode *arg1 = ast_call_argument(call, 1);
    ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
    ASTNode *world_decl = host_decl != NULL && host_decl->type == AST_WORLD_DECL
        ? host_decl
        : NULL;
    const char *zone_name = domain_query_name_arg(arg0);
    const char *state_name = domain_query_name_arg(arg1);
    ASTNode *zone_decl = world_decl != NULL
        ? transpiler_resolve_world_zone_decl(ctx, world_decl, zone_name)
        : NULL;

    if (world_decl != NULL
        && ast_call_arg_count(call) == 2
        && zone_decl != NULL
        && state_name != NULL
        && transpiler_find_zone_state_decl(zone_decl, state_name) != NULL) {
        return domain_query_heap_fmt(ctx, "self->%s.__state_%s",
            zone_name, state_name);
    }

    return domain_query_unsupported(ctx,
        "C backend: HasZoneState requires active world context with matching zone state");
}

char *
emit_builtin_domain_query(ASTNode *call, BuiltinKind bk, TranspilerCtx *ctx)
{
    switch (bk) {
    case BUILTIN_HAS_PROJECTION:
        return emit_builtin_has_projection(call, ctx);
    case BUILTIN_HAS_LAYER:
        return emit_builtin_has_layer(call, ctx);
    case BUILTIN_HAS_STATE:
        return emit_builtin_has_state(call, ctx);
    case BUILTIN_HAS_ZONE:
        return emit_builtin_has_zone(call, ctx);
    case BUILTIN_HAS_ZONE_PROJECTION:
        return emit_builtin_has_zone_projection(call, ctx);
    case BUILTIN_HAS_ZONE_LAYER:
        return emit_builtin_has_zone_layer(call, ctx);
    case BUILTIN_HAS_ZONE_STATE:
        return emit_builtin_has_zone_state(call, ctx);
    default:
        return domain_query_unsupported(ctx,
            "C backend: unsupported domain query builtin");
    }
}
