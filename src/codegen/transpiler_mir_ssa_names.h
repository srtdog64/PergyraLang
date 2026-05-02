/* Slot/view string classification is shared with non-MIR codegen paths. */
#include "codegen_slot_type_policy.h"

static const char *
transpiler_resolve_ssa_name(const TranspilerSSANameMap *ssa_map,
                            const char *base_name)
{
    size_t idx;
    size_t attempts;

    if (ssa_map == NULL || base_name == NULL)
        return NULL;
    idx = transpiler_ssa_name_bucket_index(base_name);
    for (attempts = 0; attempts < TRANSPILE_SSA_NAME_BUCKETS; ++attempts) {
        const TranspilerSSANameBucket *bucket = &ssa_map->buckets[idx];
        if (!bucket->in_use)
            return NULL;
        if (bucket->base_name != NULL && strcmp(bucket->base_name, base_name) == 0)
            return bucket->versioned_name;
        idx = (idx + 1) % TRANSPILE_SSA_NAME_BUCKETS;
    }
    return NULL;
}

static const MIRRoutine *
transpiler_find_mir_method(const TranspilerCtx *ctx,
                           const char *owner_name,
                           const ASTNode *method_decl)
{
    const char *target = NULL;

    if (ctx == NULL || ctx->mir == NULL || owner_name == NULL
        || method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->data.func_decl.name == NULL) {
        return NULL;
    }

    target = method_decl->data.func_decl.name;
    {
        const MIRDeclHeader *header = mir_find_decl_header(ctx->mir, owner_name);
        if (header != NULL) {
            for (size_t i = 0; i < header->method_metadata_count; i++) {
                const MIRDeclMethod *method = &header->method_metadata[i];
                if (method->name == NULL || strcmp(method->name, target) != 0)
                    continue;
                if (method->has_routine && method->routine_index < ctx->mir->routine_count)
                    return &ctx->mir->routines[method->routine_index];
                return NULL;
            }
            return NULL;
        }
    }

    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_METHOD)
            continue;
        if (routine->ast == method_decl)
            return routine;
        if (routine->name != NULL
            && routine->owner_name != NULL
            && strcmp(routine->name, target) == 0
            && strcmp(routine->owner_name, owner_name) == 0) {
            return routine;
        }
    }

    return NULL;
}

static bool
transpiler_name_is_token_local(const char *name);

static bool
transpiler_has_explicit_local_binding(const ASTNode *func_decl,
                                      const char *base_name);

static const char *
transpiler_resolve_active_ssa_name(const TranspilerCtx *ctx,
                                   const char *base_name)
{
    const char *resolved;
    if (ctx == NULL || ctx->active_ssa_map == NULL || base_name == NULL)
        return NULL;
    if (transpiler_is_implicit_field((TranspilerCtx *)ctx, base_name))
        return NULL;
    if (is_slot_var((TranspilerCtx *)ctx, base_name))
        return NULL;
    resolved = transpiler_resolve_ssa_name(
        (const TranspilerSSANameMap *)ctx->active_ssa_map,
        base_name);
    if (resolved != NULL)
        return resolved;
    if (transpiler_name_is_token_local(base_name))
        return base_name;
    return NULL;
}

static bool
transpiler_name_is_token_local(const char *name)
{
    const char *tag;
    if (name == NULL)
        return false;
    tag = strstr(name, "_token");
    if (tag == NULL)
        return false;
    tag += 6;
    while (*tag != '\0') {
        if (*tag < '0' || *tag > '9')
            return false;
        tag++;
    }
    return true;
}

static bool
transpiler_type_name_is_slot_like(const char *type_name)
{
    return pgy_codegen_type_name_is_slot_family(type_name);
}

static bool
transpiler_type_name_is_view_like(const char *type_name)
{
    return pgy_codegen_type_name_is_view(type_name);
}

/* Locals whose declaration is emitted alongside a slot claim (slots,
 * tokens, views) must not get an SSA pre-allocation in the MIR header
 * because the claim emit provides the real declaration. */
static bool
transpiler_type_name_is_claim_shape(const char *type_name)
{
    if (type_name == NULL)
        return false;
    return transpiler_type_name_is_slot_like(type_name)
        || strncmp(type_name, "Token<", 6) == 0;
}

static bool
transpiler_block_has_claim_for_slot_local(const MIRBasicBlock *block,
                                          const char *slot_name)
{
    if (block == NULL || slot_name == NULL)
        return false;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        const char *claim_name;
        if (inst->kind != MIR_INST_RESOURCE_OP
            || inst->name == NULL
            || strcmp(inst->name, "Claim") != 0) {
            continue;
        }
        claim_name = inst->slot_anchor != NULL ? inst->slot_anchor : inst->arg0;
        if (claim_name != NULL && strcmp(claim_name, slot_name) == 0)
            return true;
    }
    return false;
}

static bool
transpiler_mir_routine_has_explicit_cfg(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    if (routine->block_count > 1)
        return true;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_BRANCH
                || inst->kind == MIR_INST_PHI
                || inst->kind == MIR_INST_CLEANUP_EDGE) {
                return true;
            }
        }
    }
    return false;
}

static bool
transpiler_emit_mir_block_with_ssa_map(TranspilerSSANameMap *ssa_map,
                                       const MIRBasicBlock *block)
{
    const char *base_names[TRANSPILE_SSA_MAP_CAPACITY];
    const char *versioned_names[TRANSPILE_SSA_MAP_CAPACITY];
    size_t map_count = 0;

    transpiler_ssa_map_clear(ssa_map);
    if (block == NULL || block->ssa_entry_values == NULL || block->ssa_entry_value_count == 0)
        return true;
    if (!transpiler_collect_ssa_name_entries(block->ssa_entry_values, block->ssa_entry_value_count,
                                            base_names, versioned_names,
                                            TRANSPILE_SSA_MAP_CAPACITY,
                                            &map_count)) {
        transpiler_free_ssa_name_entries(base_names, map_count);
        return false;
    }
    if (!transpiler_rebuild_ssa_map(ssa_map, base_names, versioned_names, map_count))
        return false;
    transpiler_free_ssa_name_entries(base_names, map_count);
    return true;
}

static char *
transpiler_make_c_ssa_name(const char *versioned_name)
{
    if (versioned_name == NULL)
        return NULL;
    char base[128];
    size_t version = 0;
    if (!transpiler_parse_versioned_name(versioned_name, base, sizeof(base), &version))
        return pergyra_strdup(versioned_name);
    if (g_type_render_ctx != NULL && transpiler_is_implicit_field(g_type_render_ctx, base)) {
        if (current_class_has_field(g_type_render_ctx, base)) {
            return strdup_fmt(current_class_uses_self_cell(g_type_render_ctx)
                ? "self->%s"
                : "self.%s", base);
        }
        return strdup_fmt("self->%s", base);
    }
    return strdup_fmt("_pgy_ssa_%s_%zu", base, version);
}

static bool
transpiler_is_implicit_field(TranspilerCtx *ctx, const char *base_name)
{
    ASTNode *host_decl = NULL;
    bool in_zone_context = false;
    const char *host_name = NULL;

    if (ctx == NULL || base_name == NULL)
        return false;
    if (strcmp(base_name, "self") == 0)
        return false;
    if (ctx->current_func_decl != NULL
        && transpiler_has_explicit_local_binding(ctx->current_func_decl, base_name))
        return false;
    host_decl = transpiler_current_host_decl_local(ctx);
    in_zone_context = (host_decl != NULL && host_decl->type == AST_ZONE_DECL);
    host_name = transpiler_decl_name_local(host_decl);
    if (current_class_has_field(ctx, base_name))
        return true;
    if (current_relation_has_field(ctx, base_name))
        return true;
    if (current_effect_has_field(ctx, base_name))
        return true;
    if (in_zone_context) {
        if (current_zone_has_field(ctx, base_name))
            return true;
        if (lookup_typed_var(ctx, base_name) == NULL && !is_slot_var(ctx, base_name))
            return true;
    }
    if (transpiler_current_world_has_field(ctx, base_name))
        return true;
    if (!in_zone_context && host_name != NULL) {
        ASTNode *zone_decl = find_zone_decl(ctx, host_name);
        if (zone_decl != NULL) {
            for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
                ASTNode *slot = zone_decl->data.zone_decl.slots[i];
                if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name, base_name) == 0) {
                    return true;
                }
            }
            for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
                ASTNode *slot = zone_decl->data.zone_decl.layer_slots[i];
                if (slot != NULL && slot->data.zone_layer_slot.slot_name != NULL
                    && strcmp(slot->data.zone_layer_slot.slot_name, base_name) == 0) {
                    return true;
                }
            }
            for (size_t i = 0; i < zone_decl->data.zone_decl.shared_count; i++) {
                ASTNode *shared = zone_decl->data.zone_decl.shared_fields[i];
                if (shared != NULL && shared->data.party_shared.name != NULL
                    && strcmp(shared->data.party_shared.name, base_name) == 0) {
                    return true;
                }
            }
        }
        if (host_name != NULL) {
            size_t name_len = strlen(host_name);
            if (name_len > 4
                && strcmp(host_name + name_len - 4, "Zone") == 0
                && lookup_typed_var(ctx, base_name) == NULL
                && !is_slot_var(ctx, base_name)) {
                return true;
            }
        }
    }
    return false;
}

static char *
transpiler_render_ssa_name(TranspilerCtx *ctx, const char *versioned_name)
{
    char base[128];
    size_t version = 0;

    if (ctx != NULL
        && versioned_name != NULL
        && transpiler_parse_versioned_name(versioned_name, base, sizeof(base), &version)
        && transpiler_is_implicit_field(ctx, base)) {
        if (current_class_has_field(ctx, base)) {
            return strdup_fmt(current_class_uses_self_cell(ctx)
                ? "self->%s"
                : "self.%s", base);
        }
        return strdup_fmt("self->%s", base);
    }
    return transpiler_make_c_ssa_name(versioned_name);
}

static bool
transpiler_versioned_name_list_contains(const char **names,
                                        size_t count,
                                        const char *name)
{
    if (names == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < count; i++) {
        if (names[i] != NULL && strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static bool
transpiler_versioned_name_list_add(const char **names,
                                   size_t *count,
                                   size_t capacity,
                                   const char *name)
{
    if (names == NULL || count == NULL || name == NULL)
        return false;
    if (transpiler_versioned_name_list_contains(names, *count, name))
        return true;
    if (*count >= capacity)
        return false;
    names[(*count)++] = name;
    return true;
}

static bool
transpiler_c_type_uses_scalar_zero(const char *c_type)
{
    if (c_type == NULL)
        return true;
    if (strchr(c_type, '*') != NULL)
        return true;
    return strcmp(c_type, "int") == 0
           || strcmp(c_type, "int32_t") == 0
           || strcmp(c_type, "int64_t") == 0
           || strcmp(c_type, "size_t") == 0
           || strcmp(c_type, "bool") == 0
           || strcmp(c_type, "float") == 0
           || strcmp(c_type, "double") == 0;
}

#include "transpiler_mir_local_type_lookup.h"
