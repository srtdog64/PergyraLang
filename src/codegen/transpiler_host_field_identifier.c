#include "transpiler_host_field_identifier.h"

#include <string.h>

#include "../parser/ast_api.h"

#include "transpiler_decl_lookup.h"
#include "transpiler_format.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_local_binding.h"
#include "transpiler_overlay_host_fields.h"
#include "transpiler_projection.h"

static const MIRRoutine *
transpiler_host_field_current_mir_routine(const TranspilerCtx *ctx)
{
    TranspilerMIRRoutineInventory inventory;
    const char *target_name;
    const char *host_name;

    if (ctx == NULL || ctx->current_func_decl == NULL)
        return NULL;

    transpiler_active_routine_inventory(ctx, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine =
            transpiler_routine_inventory_get(&inventory, i);
        if (routine != NULL && routine->ast == ctx->current_func_decl)
            return routine;
    }
    target_name = ast_declaration_name(ctx->current_func_decl);
    host_name = transpiler_decl_name_local(ctx->current_host_decl);
    if (target_name == NULL || host_name == NULL)
        return NULL;
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine =
            transpiler_routine_inventory_get(&inventory, i);
        const char *routine_name = transpiler_mir_routine_name(routine);
        const char *owner_name = transpiler_mir_routine_owner_name(routine);
        if (routine != NULL
            && transpiler_mir_routine_kind(routine) == MIR_SCOPE_METHOD
            && routine_name != NULL
            && owner_name != NULL
            && strcmp(routine_name, target_name) == 0
            && strcmp(owner_name, host_name) == 0) {
            return routine;
        }
    }
    return NULL;
}

static bool
transpiler_mir_routine_has_param_name(const MIRRoutine *routine,
                                      const char *base_name)
{
    if (routine == NULL || base_name == NULL)
        return false;
    if (transpiler_mir_routine_has_signature(routine)) {
        for (size_t i = 0; i < transpiler_mir_routine_param_count(routine);
             i++) {
            FuncParam *param = transpiler_mir_routine_param(routine, i);
            if (param != NULL
                && param->name != NULL
                && strcmp(param->name, base_name) == 0) {
                return true;
            }
        }
    }
    return false;
}

bool
transpiler_current_function_has_self_receiver(const TranspilerCtx *ctx)
{
    const MIRRoutine *routine = transpiler_host_field_current_mir_routine(ctx);

    if (routine != NULL)
        return transpiler_mir_routine_kind(routine) == MIR_SCOPE_METHOD;
    if (ctx == NULL
        || ctx->current_func_decl == NULL
        || ctx->current_func_decl->type != AST_FUNC_DECL) {
        return false;
    }
    if (ctx->current_host_decl != NULL)
        return true;
    if (transpiler_active_has_mir(ctx))
        return false;
    for (size_t i = 0; i < ast_func_param_count(ctx->current_func_decl); i++) {
        FuncParam *param = ast_func_param(ctx->current_func_decl, i);
        if (param != NULL
            && param->name != NULL
            && strcmp(param->name, "self") == 0) {
            return true;
        }
    }
    return false;
}

static bool
transpiler_identifier_is_true_local(TranspilerCtx *ctx,
                                    const ASTNode *func_decl,
                                    const char *id_name)
{
    if (id_name == NULL || func_decl == NULL)
        return false;
    (void)ctx;
    if (func_decl->type == AST_FUNC_DECL) {
        size_t param_count = ast_func_param_count(func_decl);
        for (size_t i = 0; i < param_count; i++) {
            FuncParam *p = ast_func_param(func_decl, i);
            if (p != NULL && p->name != NULL && strcmp(p->name, id_name) == 0)
                return true;
        }
    }
    return transpiler_has_explicit_local_binding(func_decl, id_name);
}

bool
transpiler_identifier_is_current_true_local(TranspilerCtx *ctx,
                                            const char *id_name)
{
    const MIRRoutine *routine = transpiler_host_field_current_mir_routine(ctx);
    if (routine != NULL) {
        return transpiler_mir_routine_has_param_name(routine, id_name)
            || transpiler_has_explicit_body_local_binding(
                ctx != NULL ? ctx->current_func_decl : NULL, id_name);
    }
    if (transpiler_active_has_mir(ctx))
        return transpiler_has_explicit_body_local_binding(
            ctx != NULL ? ctx->current_func_decl : NULL, id_name);
    return transpiler_identifier_is_true_local(ctx,
        ctx != NULL ? ctx->current_func_decl : NULL,
        id_name);
}

static bool
transpiler_current_host_has_field(TranspilerCtx *ctx, const char *id_name)
{
    if (ctx == NULL || id_name == NULL)
        return false;
    return current_class_has_field(ctx, id_name)
        || current_party_has_field(ctx, id_name)
        || current_roster_has_field(ctx, id_name)
        || current_relation_has_field(ctx, id_name)
        || current_effect_has_field(ctx, id_name)
        || current_zone_has_field(ctx, id_name)
        || transpiler_current_world_has_field(ctx, id_name);
}

bool
transpiler_identifier_is_stale_host_field_snapshot(TranspilerCtx *ctx,
                                                   const char *id_name)
{
    return transpiler_current_host_has_field(ctx, id_name)
        && !transpiler_identifier_is_current_true_local(ctx, id_name);
}

char *
transpiler_emit_current_host_field_identifier(TranspilerCtx *ctx,
                                             const char *id_name)
{
    if (ctx == NULL || id_name == NULL || strcmp(id_name, "self") == 0)
        return NULL;
    if (current_class_has_field(ctx, id_name)) {
        return strdup_fmt(current_class_uses_self_cell(ctx)
            ? "self->%s"
            : "self.%s", id_name);
    }
    if (current_party_has_field(ctx, id_name)
        || current_roster_has_field(ctx, id_name)
        || current_relation_has_field(ctx, id_name)
        || current_effect_has_field(ctx, id_name)
        || current_zone_has_field(ctx, id_name)
        || transpiler_current_world_has_field(ctx, id_name)) {
        return strdup_fmt("self->%s", id_name);
    }
    return NULL;
}
