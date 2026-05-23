#include "type_checker_internal.h"
#include "diag_codes.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

#include <stdlib.h>

bool
type_check_overlay_decl_common(ASTNode *node,
                               SemanticContext *ctx,
                               const char *name,
                               SymbolKind kind,
                               ASTNode **shared_fields,
                               size_t shared_count,
                               ASTNode **methods,
                               size_t method_count,
                               const char *kind_name)
{
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = kind;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        existing->kind = kind;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_OVERLAY_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of %s '%s'", kind_name, name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        if (ast_party_shared_type(shared) != NULL)
            domain_lookup_shared_type_metadata(shared, ctx);
        if (ast_party_shared_initializer(shared) != NULL)
            type_check_expression(ast_party_shared_initializer(shared), ctx);
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    /* Register domain slots so bare slot access works in hosted funcs. */
    if (node->type == AST_ZONE_DECL) {
        ASTNode **slots;
        size_t slot_count;

        slots = ast_zone_slots(node, &slot_count);
        for (size_t i = 0; i < slot_count; i++) {
            ASTNode *slot = slots[i];
            const char *slot_name = ast_domain_slot_name(slot);
            if (slot != NULL && slot->type == AST_DOMAIN_SLOT
                && slot_name != NULL
                && ast_domain_slot_type(slot) != NULL) {
                Type *slot_type = domain_lookup_slot_type_metadata(slot, ctx);
                Symbol *slot_sym = calloc(1, sizeof(Symbol));
                slot_sym->name = pergyra_strdup(slot_name);
                slot_sym->kind = SYMBOL_VARIABLE;
                slot_sym->type = slot_type != NULL ? slot_type : TYPE_UNKNOWN;
                slot_sym->decl_line = slot->line;
                slot_sym->decl_col = slot->column;
                scope_declare(ctx->scope, slot_sym);
            }
        }
    }
    if (node->type == AST_WORLD_DECL) {
        ASTNode **zones;
        size_t zone_count;

        zones = ast_world_zones(node, &zone_count);
        for (size_t i = 0; i < zone_count; i++) {
            ASTNode *wz = zones[i];
            const char *slot_name = ast_world_zone_slot_name(wz);
            const char *type_name = ast_world_zone_type_name(wz);
            if (wz != NULL && wz->type == AST_WORLD_ZONE
                && slot_name != NULL
                && type_name != NULL) {
                Type *zone_type =
                    semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                        ctx, type_name, wz);
                Symbol *zone_sym = calloc(1, sizeof(Symbol));
                zone_sym->name = pergyra_strdup(slot_name);
                zone_sym->kind = SYMBOL_VARIABLE;
                zone_sym->type = zone_type != NULL ? zone_type : TYPE_UNKNOWN;
                zone_sym->decl_line = wz->line;
                zone_sym->decl_col = wz->column;
                scope_declare(ctx->scope, zone_sym);
            }
        }
    }
    /* Register shared fields so bare field access works in hosted funcs. */
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        const char *shared_name = ast_party_shared_name(shared);
        if (shared != NULL && shared_name != NULL) {
            Type *field_type = TYPE_UNKNOWN;
            if (ast_party_shared_type(shared) != NULL)
                field_type = domain_lookup_shared_type_metadata(shared, ctx);
            Symbol *field_sym = calloc(1, sizeof(Symbol));
            field_sym->name = pergyra_strdup(shared_name);
            field_sym->kind = SYMBOL_VARIABLE;
            field_sym->type = field_type;
            field_sym->decl_line = shared->line;
            field_sym->decl_col = shared->column;
            scope_declare(ctx->scope, field_sym);
        }
    }
    for (size_t i = 0; i < method_count; i++)
        type_check_func_decl(methods[i], ctx);
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}
