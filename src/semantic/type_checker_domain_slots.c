#include "type_checker_internal.h"
#include "type_checker_world_internal.h"
#include "diag_codes.h"

bool
type_check_domain_slots(ASTNode **slots,
                        size_t slot_count,
                        SemanticContext *ctx,
                        const char *kind_name)
{
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type = world_resolve_domain_slot_type(slot, ctx);
        if (ast_domain_slot_is_subject(slot)
            && !type_is_subject_type(slot_type, ctx)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, slot,
                "%s subject slot '%s' requires a subject type",
                kind_name,
                ast_domain_slot_name(slot));
        } else if (ast_domain_slot_is_vessel(slot)) {
            ASTNode *slot_decl = NULL;
            if (slot_type != NULL && slot_type->kind == TYPE_KIND_CLASS
                && slot_type->name != NULL) {
                slot_decl = find_type_decl_by_name(ctx->program_root, slot_type->name);
            }
            if (slot_decl == NULL || slot_decl->type != AST_CLASS_DECL
                || ast_class_nominal_kind(slot_decl) != NOMINAL_DECL_VESSEL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_DOMAIN_VESSEL_REQUIRED, PGY_FIX_DECLARE_VESSEL_TYPE,
                    slot,
                    "%s vessel slot '%s' requires a vessel type",
                    kind_name,
                    ast_domain_slot_name(slot));
            }
        }
    }

    return !ctx->has_error;
}

bool
type_check_domain_slot_initializers(ASTNode **slots,
                                    size_t slot_count,
                                    SemanticContext *ctx,
                                    const char *kind_name)
{
    if (slots == NULL || ctx == NULL)
        return true;

    scope_enter(&ctx->scope, SCOPE_BLOCK);

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        Symbol *sym;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || ast_domain_slot_name(slot) == NULL) {
            continue;
        }

        slot_type = world_resolve_domain_slot_type(slot, ctx);
        if (slot_type == NULL || slot_type == TYPE_UNKNOWN)
            continue;

        sym = symbol_create_variable(ast_domain_slot_name(slot), slot_type,
            slot->line, slot->column);
        if (sym == NULL)
            continue;

        if (!scope_declare(ctx->scope, sym)) {
            Symbol *existing = scope_lookup_current(ctx->scope,
                ast_domain_slot_name(slot));
            if (existing != NULL
                && existing->type != NULL
                && type_equals(existing->type, slot_type)) {
                symbol_destroy(sym);
                continue;
            }
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_REDECLARATION,
                PGY_CAUSE_DOMAIN_SLOT_DUPLICATE_NAME,
                PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
                slot,
                "Redeclaration of %s slot '%s'",
                kind_name,
                ast_domain_slot_name(slot));
            symbol_destroy(sym);
        }
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        Type *init_type;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || ast_domain_slot_initializer(slot) == NULL) {
            continue;
        }

        slot_type = world_resolve_domain_slot_type(slot, ctx);
        init_type = type_check_expression(ast_domain_slot_initializer(slot), ctx);
        if (slot_type == NULL || init_type == NULL
            || slot_type == TYPE_UNKNOWN || init_type == TYPE_UNKNOWN) {
            continue;
        }

        require_assignable(init_type, slot_type,
            ast_domain_slot_initializer(slot), ctx);
    }

    scope_exit(&ctx->scope);
    return !ctx->has_error;
}
