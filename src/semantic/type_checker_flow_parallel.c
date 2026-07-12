#include <string.h>
#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"
#include "type_checker_flow_internal.h"
#include "../parser/ast_analysis.h"

/* Writer analysis is owned by the AST layer (ast_statement_assigns_identifier)
 * so this checker and both backend capture emitters agree on who writes. */
static bool
parallel_task_assigns_name(ASTNode *node, const char *name)
{
    return ast_statement_assigns_identifier(node, name);
}

/* docs/178 Copy evidence, statement level: a reader arm of a written scalar
 * takes a pre-parallel snapshot instead of the live location. Only primitive
 * scalars are snapshot-eligible -- copying them cannot duplicate identity,
 * ownership, or synchronization state. */
static bool
parallel_scalar_snapshot_eligible(const Symbol *sym)
{
    const char *tn = sym != NULL && sym->type != NULL
        ? sym->type->name : NULL;

    if (tn == NULL)
        return false;
    return strcmp(tn, "Int") == 0
        || strcmp(tn, "Long") == 0
        || strcmp(tn, "Float") == 0
        || strcmp(tn, "Double") == 0
        || strcmp(tn, "Bool") == 0;
}

/* docs/178 WO-DOP-1 rung 0: Disjointness evidence at the parallel boundary.
 * A captured Slice binding is admitted through the collection-capture
 * reject when it is one half of a construction-guaranteed disjoint split
 * of one base array:
 *   let lo = base.Slice(0, B);  let hi = base.Slice(B, LEN);
 * with all of (fail-closed):
 *   - both halves carry recorded split facts on the same base symbol and
 *     the same boundary (symbol identity, or equal literals);
 *   - the captured fact-bearing slices of that base are exactly this pair;
 *   - each half is referenced by exactly one arm;
 *   - the base array itself is not referenced by any arm.
 * Everything else keeps the existing reject. Writes through the two views
 * then touch provably non-overlapping element ranges of live storage. */

static size_t
parallel_ref_task_count(ASTNode *node, const char *name)
{
    size_t task_count = ast_parallel_task_count(node);
    size_t refs = 0;

    for (size_t t = 0; t < task_count; t++) {
        if (ast_contains_free_identifier_ref(
                ast_parallel_task(node, t), name))
            refs++;
    }
    return refs;
}

static bool
parallel_split_boundary_matches(const Symbol *a, const Symbol *b)
{
    if (a->slice_split_info.boundary_sym != NULL
        || b->slice_split_info.boundary_sym != NULL)
        return a->slice_split_info.boundary_sym
            == b->slice_split_info.boundary_sym;
    return a->slice_split_info.boundary_lit
        == b->slice_split_info.boundary_lit;
}

static bool
parallel_disjoint_split_admitted(ASTNode *node, SemanticContext *ctx,
                                 Symbol *sym)
{
    Symbol *partner = NULL;
    size_t captured_fact_siblings = 0;

    if (node == NULL || ctx == NULL || sym == NULL || sym->name == NULL)
        return false;
    if (!sym->slice_split_info.has_fact)
        return false;
    if (parallel_ref_task_count(node, sym->name) != 1)
        return false;
    if (sym->slice_split_info.base_sym == NULL
        || sym->slice_split_info.base_sym->name == NULL
        || parallel_ref_task_count(node,
               sym->slice_split_info.base_sym->name) != 0)
        return false;

    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *other = scope->symbols[i];

            if (other == NULL || other == sym || other->name == NULL)
                continue;
            if (!other->slice_split_info.has_fact
                || other->slice_split_info.base_sym
                    != sym->slice_split_info.base_sym)
                continue;
            if (parallel_ref_task_count(node, other->name) == 0)
                continue;
            captured_fact_siblings++;
            if (other->slice_split_info.is_upper
                    != sym->slice_split_info.is_upper
                && parallel_split_boundary_matches(sym, other)
                && parallel_ref_task_count(node, other->name) == 1)
                partner = other;
        }
    }
    return captured_fact_siblings == 1 && partner != NULL;
}

static bool
parallel_reject_shared_collection_capture(ASTNode *parallel_node,
                                          ASTNode *task,
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
            /* Disjoint split halves carry their own evidence. */
            if (type_is_constructed_named(sym->type, "Slice")
                && parallel_disjoint_split_admitted(parallel_node, ctx, sym))
                continue;
            /* Index-disjointness evidence (docs/181 R1): the join
             * admission sealed this array as [binding]-only, so every
             * task touches its own element. */
            if (ast_parallel_is_index_join(parallel_node)
                && ast_parallel_join_index_array_admitted(parallel_node,
                                                          sym->name))
                continue;
            /* Snapshot-read evidence (docs/181 R5): never written and
             * only ever `name[<expr>]` reads; the emitters' fan-out
             * entry alias check closes the written-backing residual. */
            if (ast_parallel_is_index_join(parallel_node)
                && ast_parallel_join_readonly_array_admitted(parallel_node,
                                                             sym->name))
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

/* docs/178 Exclusivity evidence: a captured scalar crosses the worker
 * boundary by shared pointer (llvm_stmt_parallel_async.c stores the parent
 * alloca address; there is no copy-in yet). Sharing is only sound when the
 * boundary carries evidence -- here, exclusive access. Reject the two
 * evidence-free write races; allow the rest (single writer + no other
 * referencing arm = the value is written by exactly one task and only read
 * after the join; all-readers = immutable share). Collections have their own
 * reject; Channel/Slot/Future are runtime-synchronized, so they are skipped.
 * docs/177 F2. */
static bool
parallel_reject_scalar_write_race(ASTNode *node, SemanticContext *ctx)
{
    size_t task_count;

    if (node == NULL || ctx == NULL)
        return false;
    task_count = ast_parallel_task_count(node);
    /* This checker is the single producer of capture-disposition facts
     * (docs/178, docs/180 §6). Rows are keyed by stable boundary ID and
     * projected into MIR for both backends. Reset keeps re-checks idempotent. */
    if (!semantic_parallel_capture_facts_reset(ctx, node)) {
        semantic_error(ctx, node,
            "Capture-disposition fact boundary allocation failed");
        return true;
    }

    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];
            const char *tn;
            size_t writers = 0;
            size_t refs = 0;
            size_t writer_task = 0;

            if (sym == NULL || sym->name == NULL)
                continue;
            /* collections: owned by parallel_reject_shared_collection_capture */
            if (worker_boundary_storage_display_name(sym->type) != NULL)
                continue;
            /* runtime-synchronized transports are safe to share */
            tn = sym->type != NULL ? sym->type->name : NULL;
            if (tn != NULL
                && (strncmp(tn, "Channel", 7) == 0
                    || strncmp(tn, "Slot", 4) == 0
                    || strncmp(tn, "SecureSlot", 10) == 0
                    || strncmp(tn, "DeviceSlot", 10) == 0
                    || strncmp(tn, "Future", 6) == 0
                    || strncmp(tn, "RemoteFuture", 12) == 0))
                continue;

            for (size_t t = 0; t < task_count; t++) {
                ASTNode *task = ast_parallel_task(node, t);
                if (parallel_task_assigns_name(task, sym->name)) {
                    writers++;
                    writer_task = t;
                }
                if (ast_contains_free_identifier_ref(task, sym->name))
                    refs++;
            }

            if (writers >= 2) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PARALLEL_SLOT_RACE_RISK,
                    PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT,
                    PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
                    node,
                    "Parallel tasks share mutable variable '%s' across the worker boundary with a writer: data race.\n"
                    "Reason:\n"
                    "- the written location is shared, and\n"
                    "- two or more tasks write it concurrently (write-write race)\n"
                    "Fix:\n"
                    "- send updates through a channel, or write to a disjoint Slot per task\n"
                    "- or compute in a single task and read '%s' after the parallel join",
                    sym->name,
                    sym->name);
                return true;
            }
            if (writers == 1 && refs >= 2
                && !parallel_scalar_snapshot_eligible(sym)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PARALLEL_SLOT_RACE_RISK,
                    PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT,
                    PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
                    node,
                    "Parallel tasks share mutable variable '%s': one task writes it while another reads it (read-write race).\n"
                    "Reason:\n"
                    "- reader arms take a pre-parallel snapshot only for primitive scalars (Int/Long/Float/Double/Bool)\n"
                    "- '%s' is not snapshot-eligible, so the read would observe the writer through a shared pointer\n"
                    "Fix:\n"
                    "- send the value through a channel\n"
                    "- or project it into a primitive local before entering parallel",
                    sym->name,
                    sym->name);
                return true;
            }
            /* writers == 1 && refs >= 2 && snapshot-eligible: admitted.
             * Reader arms receive the pre-parallel value by copy (Copy
             * evidence); the writer keeps the exclusive live location
             * (Exclusivity). Both backends materialize the snapshot from
             * the fact row recorded here. docs/178. */
            if (writers == 1 && refs >= 2
                && !semantic_parallel_capture_facts_add_snapshot(
                    ctx, node, sym->name, writer_task)) {
                semantic_error(ctx, node,
                    "Capture-disposition fact allocation failed while admitting parallel snapshot");
                return true;
            }
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

    /* R4/R3 (docs/181): reduce and any joins produce one scalar; the
     * statement form would compute and silently drop it, so the shape
     * fails closed instead of becoming a lost result. */
    if ((ast_parallel_join_reduce_op(node) != NULL
         || ast_parallel_join_is_any(node))
        && !ctx->in_parallel_join_expr) {
        semantic_error(ctx, node,
            "parallel join with any/sum/product/min/max produces a value; bind it: let x = parallel (...) join with any { give <expr>; }; (docs/181 R3/R4)");
        return false;
    }

    /* Join form (docs/181 SS1 rung 0): admission first -- its replicated
     * arms reject every outer write, so the scalar race check below can
     * never record rows for it. */
    Type *join_elem_type = NULL;
    if (ast_parallel_is_join_form(node)
        && !type_check_parallel_join_admit(node, ctx, &join_elem_type))
        return false;

    if (parallel_reject_scalar_write_race(node, ctx))
        return false;
    /* Every capture disposition is now recorded. MIR imports only a sealed
     * table; a missing seal is a hard error, never backend re-derivation. */
    if (!semantic_parallel_capture_facts_seal(ctx, node)) {
        semantic_error(ctx, node,
            "Capture-disposition fact seal failed");
        return false;
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
        if (parallel_reject_shared_collection_capture(node, task, ctx))
            break;
        /* The old blanket "writes shared captured variable" warning is gone:
         * after the write-race reject and the snapshot/disjoint admissions,
         * every pattern that survives to this point is sound (single writer
         * with exclusive location, snapshot readers, disjoint views), so
         * each firing of that warning was a false data-race claim. */
        restore_resource_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        if (ast_parallel_is_join_form(node) && join_elem_type != NULL) {
            /* The element binding is body-scoped and read-only; the
             * admission above already rejected writes to it. */
            const char *elem_name = ast_parallel_join_element(node);
            Symbol *elem_sym = elem_name != NULL
                ? symbol_create_variable(elem_name, join_elem_type,
                                         node->line, node->column)
                : NULL;
            if (elem_sym != NULL)
                scope_declare(ctx->scope, elem_sym);
        }
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
