/* Array releases made by defer cleanups, admitted at every routine exit.
 * The defer body is checked once where it is written; a later move, own
 * handoff, constructor or retaining call can take the storage before the
 * cleanup runs, and that is refused at the exit the cleanup runs at. */
#include "array_storage_defer_release_owner.h"
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_release_owner.h"
#include "type_checker/type_checker_internal.h"
#include "diag_codes.h"
#include <stdlib.h>

static Scope *
defer_release_routine_scope(Scope *scope)
{
    while (scope != NULL && scope->kind != SCOPE_FUNCTION)
        scope = scope->parent;
    return scope;
}

static bool
defer_release_failure(SemanticContext *ctx, ASTNode *site, const char *name,
                      const char *reason)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        site, "A defer cleanup releases Array '%s' when the routine exits: %s",
        name != NULL ? name : "<array>", reason);
    return false;
}

bool
semantic_array_storage_record_defer_release(ASTNode *receiver,
    bool owned_strings, SemanticContext *ctx)
{
    if (ctx == NULL || !ctx->in_defer_cleanup
        || ctx->future_lifecycle_unreachable_depth != 0)
        return true;
    Scope *owner = ctx->indexed_string_defer_scope;
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(receiver, ctx) : NULL;
    if (owner == NULL || owner->kind != SCOPE_FUNCTION || binding == NULL
        || binding->decl_syntax_id == 0
        || ast_identifier_binding_syntax_id(receiver) != binding->decl_syntax_id)
        return defer_release_failure(ctx, receiver,
            binding != NULL ? binding->name : NULL,
            "the release has no named binding or routine owner");
    for (size_t i = 0; i < owner->deferred_array_release_count; i++) {
        DeferredArrayRelease *row = &owner->deferred_array_releases[i];
        if (row->receiver != receiver) continue;
        /* A block walked again declares the binding again. */
        row->settled = false;
        return true;
    }
    if (owner->deferred_array_release_count == owner->deferred_array_release_capacity) {
        size_t capacity = owner->deferred_array_release_capacity;
        size_t next = capacity == 0 ? 4 : capacity * 2;
        DeferredArrayRelease *rows = next <= capacity
            || next > SIZE_MAX / sizeof(*rows) ? NULL
            : realloc(owner->deferred_array_releases, next * sizeof(*rows));
        if (rows == NULL)
            return defer_release_failure(ctx, receiver, binding->name,
                "the release record could not be allocated");
        owner->deferred_array_releases = rows;
        owner->deferred_array_release_capacity = next;
    }
    owner->deferred_array_releases[owner->deferred_array_release_count++] =
        (DeferredArrayRelease){receiver, binding->decl_syntax_id, owned_strings, false};
    return true;
}

bool
semantic_array_storage_admit_move_before_defer_release(
    ASTNode *source, SemanticContext *ctx)
{
    if (ctx == NULL || source == NULL || source->type != AST_IDENTIFIER)
        return true;
    Scope *owner = defer_release_routine_scope(ctx->scope);
    Symbol *binding = lookup_identifier_symbol(source, ctx);
    uint32_t binding_id = ast_identifier_binding_syntax_id(source);
    if (owner == NULL || binding == NULL || binding_id == 0
        || binding->decl_syntax_id != binding_id)
        return true;
    for (size_t i = 0; i < owner->deferred_array_release_count; i++) {
        const DeferredArrayRelease *row = &owner->deferred_array_releases[i];
        if (row->settled || row->binding_id != binding_id)
            continue;
        return defer_release_failure(ctx, source, binding->name,
            "this move would transfer its storage before the cleanup runs");
    }
    return true;
}

/* The conditions a release written at this exit must meet. */
static bool
defer_release_admit(SemanticContext *ctx, ASTNode *site, const ASTNode *returned,
                    const DeferredArrayRelease *row, Symbol *binding)
{
    const char *name = ast_identifier_name(row->receiver);
    /* A block has no position of its own; the defer's use names it. */
    if (site == NULL || site->line == 0)
        site = row->receiver;
    if (binding == NULL)
        return defer_release_failure(ctx, site, name,
            "its binding is not in scope at this exit");
    /* Returning the binding hands its storage to the caller. */
    if (returned != NULL && returned->type == AST_IDENTIFIER
        && ast_identifier_binding_syntax_id(returned) == row->binding_id)
        return defer_release_failure(ctx, site, name,
            "this return hands its storage to the caller");
    /* Exclusivity was proved where the defer was written. A later call that
     * may keep the storage marks it escaped or leaves a pending handoff. */
    if (binding->is_consumed || binding->has_escaped_array_storage)
        return defer_release_failure(ctx, site, name,
            "its storage was moved, handed off or kept by a call after the defer");
    bool pending = semantic_array_storage_has_pending(binding)
        || (row->owned_strings && semantic_array_storage_has_pending_elements(ctx,
            binding->decl_syntax_id));
    if (!pending) return true;
    /* Calls whose preservation is decided after Pass 2 are judged then,
     * with this exit's place in their order. */
    if (!semantic_array_storage_record_pending_drop(ctx, row->receiver,
            binding, row->owned_strings ? ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS
                : ARRAY_STORAGE_RELEASE_DROP_STORAGE))
        return defer_release_failure(ctx, site, name,
            "the calls before this exit have no current preservation record");
    return true;
}

static bool
defer_release_exit_skipped(const SemanticContext *ctx)
{
    return ctx == NULL || ctx->has_error || ctx->in_defer_cleanup
        || ctx->future_lifecycle_unreachable_depth != 0;
}

bool
semantic_array_storage_defer_releases_at_exit(ASTNode *site,
    const ASTNode *returned, SemanticContext *ctx)
{
    if (defer_release_exit_skipped(ctx)) return ctx == NULL || !ctx->has_error;
    Scope *owner = defer_release_routine_scope(ctx->scope);
    if (owner == NULL) return true;
    for (size_t i = 0; i < owner->deferred_array_release_count; i++) {
        const DeferredArrayRelease *row = &owner->deferred_array_releases[i];
        if (row->settled) continue;
        Symbol *binding = NULL;
        for (Scope *s = ctx->scope; s != NULL && binding == NULL; s = s == owner ? NULL : s->parent)
            for (size_t j = 0; j < s->symbol_count && binding == NULL; j++)
                if (s->symbols[j] != NULL && s->symbols[j]->decl_syntax_id == row->binding_id)
                    binding = s->symbols[j];
        if (!defer_release_admit(ctx, site, returned, row, binding)) return false;
    }
    return true;
}

bool
semantic_array_storage_defer_releases_at_block_exit(ASTNode *site, SemanticContext *ctx)
{
    if (defer_release_exit_skipped(ctx)) return ctx == NULL || !ctx->has_error;
    Scope *block = ctx->scope;
    Scope *owner = defer_release_routine_scope(block);
    if (block == NULL || owner == NULL || block == owner) return true;
    for (size_t i = 0; i < owner->deferred_array_release_count; i++) {
        DeferredArrayRelease *row = &owner->deferred_array_releases[i];
        for (size_t j = 0; !row->settled && j < block->symbol_count; j++) {
            Symbol *binding = block->symbols[j];
            if (binding == NULL || binding->decl_syntax_id != row->binding_id) continue;
            if (!defer_release_admit(ctx, site, NULL, row, binding)) return false;
            row->settled = true;
        }
    }
    return true;
}
