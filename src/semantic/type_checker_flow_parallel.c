#include <string.h>
#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"
#include "type_checker_flow_internal.h"
#include "../parser/ast_analysis.h"

static ASTNode *
parallel_assign_target_root(ASTNode *target)
{
    while (target != NULL) {
        if (target->type == AST_MEMBER_ACCESS)
            target = ast_member_object(target);
        else if (target->type == AST_ARRAY_ACCESS)
            target = ast_array_access_array(target);
        else
            break;
    }
    return target;
}

static bool
parallel_task_assigns_name(ASTNode *node, const char *name)
{
    if (node == NULL || name == NULL)
        return false;
    switch (node->type) {
    case AST_ASSIGNMENT: {
        ASTNode *root =
            parallel_assign_target_root(ast_assignment_target(node));
        if (root != NULL && root->type == AST_IDENTIFIER) {
            const char *tn = ast_identifier_name(root);
            if (tn != NULL && strcmp(tn, name) == 0)
                return true;
        }
        return parallel_task_assigns_name(ast_assignment_value(node), name);
    }
    case AST_BLOCK: {
        size_t n = ast_block_statement_count(node);
        for (size_t i = 0; i < n; i++)
            if (parallel_task_assigns_name(ast_block_statement(node, i), name))
                return true;
        return false;
    }
    case AST_IF_STMT:
        return parallel_task_assigns_name(ast_if_then_branch(node), name)
            || parallel_task_assigns_name(ast_if_else_branch(node), name);
    case AST_WHILE_LOOP:
        return parallel_task_assigns_name(ast_while_body(node), name);
    case AST_FOR_LOOP:
        return parallel_task_assigns_name(ast_for_body(node), name);
    default:
        return false;
    }
}

static bool
parallel_reject_shared_collection_capture(ASTNode *task,
                                          SemanticContext *ctx)
{
    if (task == NULL || ctx == NULL)
        return false;

    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];
            const char *kind = sym != NULL
                ? worker_boundary_storage_display_name(sym->type)
                : NULL;
            if (kind == NULL || sym->name == NULL)
                continue;
            if (!ast_contains_free_identifier_ref(task, sym->name))
                continue;

            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
                task,
                "Parallel task cannot capture mutable collection '%s' (%s<T>) by shared pointer.\n"
                "Reason:\n"
                "- Array/Slice/List/Queue/Set/HashMap storage can grow, rehash, or alias during task execution\n"
                "- concurrent worker access would make the generated C/LLVM pointer handoff undefined behavior\n"
                "Fix:\n"
                "- copy the collection before entering parallel\n"
                "- or send values through a channel/result boundary",
                sym->name,
                kind);
            return true;
        }
    }
    return false;
}

bool
type_check_parallel_block(ASTNode *node, SemanticContext *ctx)
{
    return type_check_parallel_block_flow(node, ctx);
}

bool
type_check_defer_stmt(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL || node->type != AST_DEFER_STMT)
        return true;
    return type_check_defer_body_flow(ast_defer_body(node), ctx);
}

bool
type_check_defer_body_flow(ASTNode *body, SemanticContext *ctx)
{
    if (body != NULL) {
        ResourceConsumeSnapshot before_defer = snapshot_resource_states(ctx);
        if (!before_defer.valid) {
            semantic_error(ctx, body,
                "Resource snapshot allocation failed while checking defer cleanup");
            destroy_resource_snapshot(&before_defer);
            return false;
        }
        if (semantic_reject_active_slot_view_boundary(body, ctx,
                "defer cleanup boundary",
                "defer executes after the current statement frontier and may run after the pin scope has ended",
                "move defer")) {
            destroy_resource_snapshot(&before_defer);
            return false;
        }
        (void)type_check_block_flow(body, ctx, NULL);
        restore_resource_states(&before_defer);
        destroy_resource_snapshot(&before_defer);
    }
    return ctx == NULL || !ctx->has_error;
}

bool
type_check_parallel_block_flow(ASTNode *node, SemanticContext *ctx)
{
    bool prev_parallel;
    ResourceConsumeSnapshot base = {0};
    ResourceConsumeSnapshot joined = {0};
    bool has_joined = false;

    if (node == NULL || ctx == NULL || node->type != AST_PARALLEL_BLOCK)
        return ctx == NULL || !ctx->has_error;

    {
        const char *view_name = NULL;
        const char *view_kind = NULL;
        const char *source_slot = NULL;
        if (semantic_find_active_slot_view(ctx->scope, &view_name,
                                           &view_kind, &source_slot)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                PGY_FIX_SERIALIZE_PIN_ACCESS,
                node,
                "Pinned view '%s' cannot cross a parallel boundary.\n"
                "Reason:\n"
                "- %s for slot '%s' is scoped to the current sequential frontier\n"
                "- parallel tasks would make read/write aliasing and cleanup order path-dependent\n"
                "Fix:\n"
                "- end the view scope before entering parallel\n"
                "- or acquire disjoint views inside serialized code",
                view_name != NULL ? view_name : "<view>",
                view_kind != NULL ? view_kind : "View",
                source_slot != NULL ? source_slot : "<slot>");
            return false;
        }
    }

    prev_parallel = ctx->in_parallel;
    ctx->in_parallel = true;
    base = snapshot_resource_states(ctx);
    if (!base.valid) {
        semantic_error(ctx, node,
            "Resource snapshot allocation failed before parallel analysis");
        ctx->in_parallel = prev_parallel;
        destroy_resource_snapshot(&base);
        return false;
    }

    for (size_t i = 0; i < ast_parallel_task_count(node); i++) {
        ASTNode *task = ast_parallel_task(node, i);
        ResourceConsumeSnapshot task_snap = {0};
        if (parallel_reject_shared_collection_capture(task, ctx))
            break;
        for (Scope *dr_scope = ctx->scope; dr_scope != NULL;
             dr_scope = dr_scope->parent) {
            for (size_t si = 0; si < dr_scope->symbol_count; si++) {
                Symbol *dsym = dr_scope->symbols[si];
                const char *tname;
                if (dsym == NULL || dsym->name == NULL)
                    continue;
                tname = dsym->type != NULL ? dsym->type->name : NULL;
                if (tname != NULL && strncmp(tname, "Channel", 7) == 0)
                    continue;
                if (!parallel_task_assigns_name(task, dsym->name))
                    continue;
                semantic_warning_code(ctx,
                    PGY_CODE_SEM_PARALLEL_SLOT_RACE_RISK,
                    task,
                    "Parallel task writes shared captured variable '%s'; concurrent tasks mutating shared state is a data race. Send updates through a channel or give each task a disjoint owner.",
                    dsym->name);
            }
        }
        restore_resource_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        (void)type_check_statement_flow(task, ctx, NULL);
        task_snap = snapshot_resource_states_from_scope(
            ctx->scope != NULL ? ctx->scope->parent : NULL, ctx);
        if (!task_snap.valid) {
            semantic_error(ctx, task != NULL ? task : node,
                "Resource snapshot allocation failed while checking parallel task");
            restore_resource_states(&base);
            scope_exit(&ctx->scope);
            destroy_resource_snapshot(&task_snap);
            break;
        }
        restore_resource_states(&base);
        scope_exit(&ctx->scope);
        resource_snapshot_record_parallel_boundary_witness(
            &base,
            has_joined ? &joined : NULL,
            &task_snap,
            ctx);
        if (has_joined) {
            const Symbol *conflict = NULL;
            if (resource_snapshot_has_parallel_conflict(&base, &joined,
                    &task_snap, &conflict)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PARALLEL_SLOT_CONFLICT,
                    PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT,
                    PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
                    task,
                    "Parallel context slot conflict on '%s': multiple tasks mutate or release the same slot. Parallel tasks cannot consume the same resource/boundary.\n"
                    "Reason:\n"
                    "- each parallel task is checked from the same entry ownership snapshot\n"
                    "- more than one task moves or releases the same ownership-bearing value\n"
                    "- the compiler cannot prove a single owner after the parallel join\n"
                    "Fix:\n"
                    "- move the value before entering parallel and pass disjoint handles\n"
                    "- or serialize the ownership-consuming work outside the parallel block",
                    conflict != NULL && conflict->name != NULL
                        ? conflict->name
                        : "<resource>");
            } else if (resource_snapshot_has_parallel_race_risk(&base,
                    &joined, &task_snap, &conflict)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PARALLEL_SLOT_RACE_RISK,
                    PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT,
                    PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
                    task,
                    "Parallel context race risk on '%s': one task reads while another mutates or releases the same slot.\n"
                    "Reason:\n"
                    "- boundary witness op_guard requires no current writer for reads\n"
                    "- writes/releases require no current access at all\n"
                    "- accepting this task pair would violate the data-race-free witness discipline\n"
                    "Fix:\n"
                    "- move the read before or after the parallel block\n"
                    "- or split the slot so each task owns a disjoint boundary",
                    conflict != NULL && conflict->name != NULL
                        ? conflict->name
                        : "<resource>");
            }
        }
        merge_resource_snapshots_or(&joined, &has_joined, &task_snap);
        if (has_joined && !joined.valid) {
            semantic_error(ctx, task != NULL ? task : node,
                "Resource snapshot merge failed while checking parallel tasks");
        }
        destroy_resource_snapshot(&task_snap);
        if (ctx->has_error)
            break;
    }

    if (has_joined)
        restore_resource_states(&joined);
    else
        restore_resource_states(&base);

    ctx->in_parallel = prev_parallel;
    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&joined);
    return !ctx->has_error;
}
