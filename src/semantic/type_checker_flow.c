/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker control-flow and ownership analysis
 */

#include <stdlib.h>
#include <string.h>
#include "type_checker_internal.h"

typedef struct
{
    Symbol              **symbols;
    bool                 *states;      /* is_consumed flags */
    QubitSemanticState   *sem_states;  /* richer resource state; currently qubit-only */
    int32_t              *pool_ids;    /* entangle pool IDs */
    size_t                count;
} ResourceConsumeSnapshot;

typedef enum
{
    FLOW_NONE        = 0,
    FLOW_FALLTHROUGH = 1 << 0,
    FLOW_BREAK       = 1 << 1,
    FLOW_CONTINUE    = 1 << 2,
    FLOW_RETURN      = 1 << 3
} FlowFlags;

typedef struct
{
    ResourceConsumeSnapshot break_states;
    ResourceConsumeSnapshot continue_states;
    bool                 has_break_states;
    bool                 has_continue_states;
    Scope               *loop_scope;
} LoopFlowState;

static FlowFlags type_check_statement_flow(ASTNode *node,
                                           SemanticContext *ctx,
                                           LoopFlowState *loop_flow);
static FlowFlags type_check_block_flow(ASTNode *node,
                                       SemanticContext *ctx,
                                       LoopFlowState *loop_flow);
static FlowFlags type_check_if_stmt_flow(ASTNode *node,
                                         SemanticContext *ctx,
                                         LoopFlowState *loop_flow);
static FlowFlags type_check_match_stmt_flow(ASTNode *node,
                                            SemanticContext *ctx,
                                            LoopFlowState *loop_flow);
static FlowFlags type_check_with_stmt_flow(ASTNode *node,
                                           SemanticContext *ctx,
                                           LoopFlowState *loop_flow);

static ResourceConsumeSnapshot
snapshot_resource_states_from_scope(Scope *scope)
{
    ResourceConsumeSnapshot snap = {0};

    while (scope != NULL) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];
            if (sym == NULL || !type_is_qubit(sym->type))
                continue;

            Symbol **new_symbols = realloc(snap.symbols,
                (snap.count + 1) * sizeof(Symbol *));
            bool *new_states = realloc(snap.states,
                (snap.count + 1) * sizeof(bool));
            QubitSemanticState *new_sem = realloc(snap.sem_states,
                (snap.count + 1) * sizeof(QubitSemanticState));
            int32_t *new_pools = realloc(snap.pool_ids,
                (snap.count + 1) * sizeof(int32_t));
            if (new_symbols == NULL || new_states == NULL
                || new_sem == NULL || new_pools == NULL) {
                free(new_symbols);
                free(new_states);
                free(new_sem);
                free(new_pools);
                free(snap.symbols);
                free(snap.states);
                free(snap.sem_states);
                free(snap.pool_ids);
                snap.symbols = NULL;
                snap.states = NULL;
                snap.sem_states = NULL;
                snap.pool_ids = NULL;
                snap.count = 0;
                return snap;
            }

            snap.symbols = new_symbols;
            snap.states = new_states;
            snap.sem_states = new_sem;
            snap.pool_ids = new_pools;
            snap.symbols[snap.count] = sym;
            snap.states[snap.count] = sym->is_consumed;
            snap.sem_states[snap.count] = sym->qubit_info.semantic_state;
            snap.pool_ids[snap.count] = sym->qubit_info.entangle_pool_id;
            snap.count++;
        }
        scope = scope->parent;
    }

    return snap;
}

static void
restore_resource_states(const ResourceConsumeSnapshot *snap)
{
    if (snap == NULL)
        return;
    for (size_t i = 0; i < snap->count; i++) {
        if (snap->symbols[i] != NULL) {
            snap->symbols[i]->is_consumed = snap->states[i];
            snap->symbols[i]->qubit_info.semantic_state = snap->sem_states[i];
            snap->symbols[i]->qubit_info.entangle_pool_id = snap->pool_ids[i];
        }
    }
}

static ResourceConsumeSnapshot
snapshot_resource_states(SemanticContext *ctx)
{
    return snapshot_resource_states_from_scope(ctx != NULL ? ctx->scope : NULL);
}

static void
merge_resource_states_or(ResourceConsumeSnapshot *dst,
                         const ResourceConsumeSnapshot *src)
{
    if (dst == NULL || src == NULL)
        return;
    size_t count = dst->count < src->count ? dst->count : src->count;
    for (size_t i = 0; i < count; i++) {
        dst->states[i] = dst->states[i] || src->states[i];
        /* Merge semantic state: take the most advanced (conservative).
         * This correctly blocks gate operations on potentially-collapsed
         * qubits, which is the primary safety property.  A known
         * limitation: IntoClassical may pass at the join point even if
         * one branch did not Measure — a full lattice join would solve
         * this but is deferred for now. */
        if (src->sem_states[i] > dst->sem_states[i])
            dst->sem_states[i] = src->sem_states[i];
        /* Merge pool IDs: keep the one that exists, or dst if both exist */
        if (dst->pool_ids[i] < 0)
            dst->pool_ids[i] = src->pool_ids[i];
    }
}

static void
destroy_resource_snapshot(ResourceConsumeSnapshot *snap)
{
    if (snap == NULL)
        return;
    free(snap->symbols);
    free(snap->states);
    free(snap->sem_states);
    free(snap->pool_ids);
    snap->symbols = NULL;
    snap->states = NULL;
    snap->sem_states = NULL;
    snap->pool_ids = NULL;
    snap->count = 0;
}

static bool
match_pattern_is_named_variant(ASTNode *pat, const char **name_out,
                               ASTNode ***args_out, size_t *arg_count_out)
{
    *name_out = NULL;
    *args_out = NULL;
    *arg_count_out = 0;

    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        *name_out = pat->data.identifier.name;
        return *name_out != NULL;
    }

    if (pat->type == AST_MEMBER_ACCESS
        && pat->data.member.object != NULL
        && pat->data.member.object->type == AST_IDENTIFIER
        && pat->data.member.name != NULL) {
        *name_out = pat->data.member.name;
        return true;
    }

    if (pat->type != AST_CALL || pat->data.call.callee == NULL) {
        return false;
    }

    if (pat->data.call.callee->type == AST_IDENTIFIER
        && pat->data.call.callee->data.identifier.name != NULL) {
        *name_out = pat->data.call.callee->data.identifier.name;
    } else if (pat->data.call.callee->type == AST_MEMBER_ACCESS
        && pat->data.call.callee->data.member.object != NULL
        && pat->data.call.callee->data.member.object->type == AST_IDENTIFIER
        && pat->data.call.callee->data.member.name != NULL) {
        *name_out = pat->data.call.callee->data.member.name;
    } else {
        return false;
    }

    *args_out = pat->data.call.arguments;
    *arg_count_out = pat->data.call.arg_count;
    return true;
}

static ASTNode *
find_enum_decl_for_type(SemanticContext *ctx, const Type *type)
{
    ASTNode *prog;

    if (ctx == NULL || type == NULL || type->kind != TYPE_KIND_ENUM
        || type->name == NULL || ctx->program_root == NULL)
        return NULL;

    prog = ctx->program_root;
    if (prog->type != AST_PROGRAM)
        return NULL;

    for (size_t i = 0; i < prog->data.program.count; i++) {
        ASTNode *stmt = prog->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ENUM_DECL
            || stmt->data.enum_decl.name == NULL)
            continue;
        if (strcmp(stmt->data.enum_decl.name, type->name) == 0)
            return stmt;
    }

    return NULL;
}

static bool
declare_match_binding(SemanticContext *ctx, ASTNode *binding_node, Type *binding_type)
{
    const char *name;
    Symbol *binding;

    if (ctx == NULL || binding_node == NULL || binding_type == NULL)
        return false;
    if (binding_node->type != AST_IDENTIFIER
        || binding_node->data.identifier.name == NULL) {
        semantic_error(ctx, binding_node,
            "Destructuring pattern currently requires identifier bindings");
        return false;
    }

    name = binding_node->data.identifier.name;
    if (scope_lookup_current(ctx->scope, name) != NULL) {
        semantic_error(ctx, binding_node,
            "Duplicate match binding '%s' in the same case scope", name);
        return false;
    }

    binding = symbol_create_variable(name, binding_type,
        binding_node->line, binding_node->column);
    if (binding == NULL || !scope_declare(ctx->scope, binding)) {
        semantic_error(ctx, binding_node,
            "Failed to declare match binding '%s'", name);
        return false;
    }

    return true;
}

static bool
type_check_special_match_pattern(ASTNode *pat, Type *subj_type,
                                 SemanticContext *ctx, bool *handled)
{
    const char *variant = NULL;
    ASTNode **args = NULL;
    size_t arg_count = 0;

    *handled = false;

    if (!match_pattern_is_named_variant(pat, &variant, &args, &arg_count)
        || variant == NULL || subj_type == NULL) {
        return true;
    }

    if (type_is_constructed_named(subj_type, "Option")) {
        Type *inner = type_get_constructed_arg(subj_type, 0);
        *handled = true;

        if (strcmp(variant, "Some") == 0) {
            if (arg_count != 1) {
                semantic_error(ctx, pat,
                    "Some pattern requires exactly one binding");
                return false;
            }
            return declare_match_binding(ctx, args[0], inner);
        }
        if (strcmp(variant, "None") == 0) {
            if (arg_count != 0) {
                semantic_error(ctx, pat,
                    "None pattern does not take payload bindings");
                return false;
            }
            return true;
        }

        semantic_error(ctx, pat,
            "Option<T> match only supports Some(...) and None patterns");
        return false;
    }

    if (type_is_constructed_named(subj_type, "Result")) {
        *handled = true;

        if (strcmp(variant, "Ok") == 0) {
            if (arg_count != 1) {
                semantic_error(ctx, pat,
                    "Ok pattern requires exactly one binding");
                return false;
            }
            return declare_match_binding(ctx, args[0],
                type_get_constructed_arg(subj_type, 0));
        }
        if (strcmp(variant, "Err") == 0) {
            if (arg_count != 1) {
                semantic_error(ctx, pat,
                    "Err pattern requires exactly one binding");
                return false;
            }
            return declare_match_binding(ctx, args[0], TYPE_STRING);
        }

        semantic_error(ctx, pat,
            "Result<T> match only supports Ok(...) and Err(...) patterns");
        return false;
    }

    if (subj_type->kind == TYPE_KIND_ENUM) {
        ASTNode *enum_decl = find_enum_decl_for_type(ctx, subj_type);
        Symbol *variant_sym;

        *handled = true;
        if (enum_decl == NULL)
            return true;

        for (size_t i = 0; i < enum_decl->data.enum_decl.variant_count; i++) {
            const char *enum_variant = enum_decl->data.enum_decl.variants[i];
            size_t param_count = enum_decl->data.enum_decl.variant_param_counts != NULL
                ? enum_decl->data.enum_decl.variant_param_counts[i] : 0;

            if (enum_variant == NULL || strcmp(enum_variant, variant) != 0)
                continue;

            if (arg_count != param_count) {
                semantic_error(ctx, pat,
                    "Enum variant '%s' expects %zu payload bindings, got %zu",
                    variant, param_count, arg_count);
                return false;
            }

            if (param_count == 0)
                return true;

            variant_sym = scope_lookup(ctx->scope, variant);
            if (variant_sym == NULL || variant_sym->type == NULL
                || variant_sym->type->kind != TYPE_KIND_FUNCTION) {
                semantic_error(ctx, pat,
                    "Enum variant '%s' is missing constructor type information",
                    variant);
                return false;
            }

            for (size_t p = 0; p < param_count; p++) {
                Type *binding_type =
                    (p < variant_sym->type->data.function.param_count)
                    ? variant_sym->type->data.function.param_types[p]
                    : TYPE_UNKNOWN;
                if (!declare_match_binding(ctx, args[p], binding_type))
                    return false;
            }
            return true;
        }

        semantic_error(ctx, pat,
            "Unknown enum variant '%s' for match subject '%s'",
            variant, subj_type->name != NULL ? subj_type->name : "<enum>");
        return false;
    }

    return true;
}

static bool
pattern_covers_variant(ASTNode *pat, const Type *subj_type,
                       const char *variant_name)
{
    const char *name = NULL;
    ASTNode **args = NULL;
    size_t arg_count = 0;

    if (pat == NULL || subj_type == NULL || variant_name == NULL)
        return false;
    if (!match_pattern_is_named_variant(pat, &name, &args, &arg_count)
        || name == NULL || strcmp(name, variant_name) != 0) {
        return false;
    }

    if (type_is_constructed_named(subj_type, "Option")) {
        if (strcmp(variant_name, "Some") == 0)
            return arg_count == 1;
        if (strcmp(variant_name, "None") == 0)
            return arg_count == 0;
        return false;
    }

    if (type_is_constructed_named(subj_type, "Result")) {
        if (strcmp(variant_name, "Ok") == 0 || strcmp(variant_name, "Err") == 0)
            return arg_count == 1;
        return false;
    }

    return subj_type->kind == TYPE_KIND_ENUM;
}

static bool
case_list_covers_variant(ASTNode *node, const Type *subj_type,
                         const char *variant_name)
{
    if (node == NULL || subj_type == NULL || variant_name == NULL)
        return false;

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        if (mc == NULL || mc->type != AST_MATCH_CASE)
            continue;
        if (mc->data.match_case.guard != NULL)
            continue;
        if (pattern_covers_variant(mc->data.match_case.pattern, subj_type, variant_name))
            return true;
    }

    return false;
}

static void
append_missing_variant(char *buf, size_t buf_size, const char *variant_name, bool *first)
{
    if (buf == NULL || buf_size == 0 || variant_name == NULL || first == NULL)
        return;

    if (!*first)
        strncat(buf, ", ", buf_size - strlen(buf) - 1);
    strncat(buf, variant_name, buf_size - strlen(buf) - 1);
    *first = false;
}

static size_t
collect_match_variant_space(const Type *subj_type, SemanticContext *ctx,
                            const char ***variants_out)
{
    static const char *option_variants[] = { "Some", "None" };
    static const char *result_variants[] = { "Ok", "Err" };

    *variants_out = NULL;

    if (subj_type == NULL)
        return 0;
    if (type_is_constructed_named(subj_type, "Option")) {
        *variants_out = option_variants;
        return 2;
    }
    if (type_is_constructed_named(subj_type, "Result")) {
        *variants_out = result_variants;
        return 2;
    }
    if (subj_type->kind == TYPE_KIND_ENUM) {
        ASTNode *enum_decl = find_enum_decl_for_type(ctx, subj_type);
        if (enum_decl == NULL)
            return 0;
        *variants_out = (const char **)enum_decl->data.enum_decl.variants;
        return enum_decl->data.enum_decl.variant_count;
    }

    return 0;
}

static void
check_match_redundancy(ASTNode *node, Type *subj_type, SemanticContext *ctx)
{
    const char **variants = NULL;
    size_t variant_count = collect_match_variant_space(subj_type, ctx, &variants);
    bool *seen;
    size_t covered = 0;

    if (node == NULL || subj_type == NULL || ctx == NULL
        || variants == NULL || variant_count == 0)
        return;

    seen = calloc(variant_count, sizeof(bool));
    if (seen == NULL)
        return;

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        if (mc == NULL || mc->type != AST_MATCH_CASE
            || mc->data.match_case.guard != NULL) {
            continue;
        }

        for (size_t v = 0; v < variant_count; v++) {
            const char *variant = variants[v];
            if (variant == NULL)
                continue;
            if (!pattern_covers_variant(mc->data.match_case.pattern, subj_type, variant))
                continue;

            if (seen[v]) {
                semantic_warning(ctx, mc->data.match_case.pattern,
                    "Redundant match case for '%s'; an earlier case already covers it",
                    variant);
            } else {
                seen[v] = true;
                covered++;
            }
            break;
        }
    }

    if (node->data.match_stmt.default_body != NULL && covered == variant_count) {
        semantic_warning(ctx, node->data.match_stmt.default_body,
            "Redundant default case; previous cases already cover all variants of '%s'",
            subj_type->name != NULL ? subj_type->name : "<unknown>");
    }

    free(seen);
}

static void
check_match_exhaustiveness(ASTNode *node, Type *subj_type, SemanticContext *ctx)
{
    char missing[256] = {0};
    bool first = true;
    bool found_missing = false;
    const char **variants = NULL;
    size_t variant_count = collect_match_variant_space(subj_type, ctx, &variants);

    if (node == NULL || subj_type == NULL || ctx == NULL)
        return;
    if (node->data.match_stmt.default_body != NULL)
        return;

    for (size_t i = 0; i < variant_count; i++) {
        const char *variant = variants[i];
        if (variant == NULL)
            continue;
        if (!case_list_covers_variant(node, subj_type, variant)) {
            append_missing_variant(missing, sizeof(missing), variant, &first);
            found_missing = true;
        }
    }

    if (found_missing) {
        semantic_error(ctx, node,
            "Non-exhaustive match for '%s'; missing cases: %s",
            subj_type->name != NULL ? subj_type->name : "<unknown>",
            missing);
    }
}

static bool
resource_snapshots_equal(const ResourceConsumeSnapshot *a,
                         const ResourceConsumeSnapshot *b)
{
    if (a == NULL || b == NULL)
        return a == b;
    if (a->count != b->count)
        return false;
    for (size_t i = 0; i < a->count; i++) {
        if (a->symbols[i] != b->symbols[i])
            return false;
        if (a->states[i] != b->states[i])
            return false;
        if (a->sem_states[i] != b->sem_states[i])
            return false;
        if (a->pool_ids[i] != b->pool_ids[i])
            return false;
    }
    return true;
}

static size_t
for_loop_known_iteration_cap(const ASTNode *node, bool *known)
{
    if (known != NULL)
        *known = false;
    if (node == NULL
        || node->data.for_loop.range_start == NULL
        || node->data.for_loop.range_end == NULL) {
        return 0;
    }
    if (node->data.for_loop.range_start->type != AST_NUMBER
        || node->data.for_loop.range_end->type != AST_NUMBER) {
        return 0;
    }

    double start = node->data.for_loop.range_start->data.number.value;
    double end = node->data.for_loop.range_end->data.number.value;
    if (known != NULL)
        *known = true;
    if (end <= start)
        return 0;
    if ((end - start) <= 1.0)
        return 1;
    return 2;
}

static ResourceConsumeSnapshot
copy_resource_snapshot(const ResourceConsumeSnapshot *src)
{
    ResourceConsumeSnapshot dst = {0};
    if (src == NULL || src->count == 0)
        return dst;

    dst.symbols    = calloc(src->count, sizeof(Symbol *));
    dst.states     = calloc(src->count, sizeof(bool));
    dst.sem_states = calloc(src->count, sizeof(QubitSemanticState));
    dst.pool_ids   = calloc(src->count, sizeof(int32_t));
    if (dst.symbols == NULL || dst.states == NULL
        || dst.sem_states == NULL || dst.pool_ids == NULL) {
        free(dst.symbols);
        free(dst.states);
        free(dst.sem_states);
        free(dst.pool_ids);
        dst.symbols = NULL;
        dst.states = NULL;
        dst.sem_states = NULL;
        dst.pool_ids = NULL;
        return dst;
    }

    memcpy(dst.symbols, src->symbols, src->count * sizeof(Symbol *));
    memcpy(dst.states, src->states, src->count * sizeof(bool));
    memcpy(dst.sem_states, src->sem_states, src->count * sizeof(QubitSemanticState));
    memcpy(dst.pool_ids, src->pool_ids, src->count * sizeof(int32_t));
    dst.count = src->count;
    return dst;
}

static void
merge_resource_snapshots_or(ResourceConsumeSnapshot *dst,
                            bool *dst_initialized,
                            const ResourceConsumeSnapshot *src)
{
    if (dst == NULL || dst_initialized == NULL || src == NULL)
        return;

    if (!*dst_initialized) {
        *dst = copy_resource_snapshot(src);
        *dst_initialized = true;
        return;
    }

    merge_resource_states_or(dst, src);
}

static void
loop_flow_record(LoopFlowState *loop_flow,
                 bool is_break,
                 const ResourceConsumeSnapshot *state)
{
    if (loop_flow == NULL || state == NULL)
        return;

    if (is_break) {
        merge_resource_snapshots_or(&loop_flow->break_states,
                                    &loop_flow->has_break_states,
                                    state);
        return;
    }

    merge_resource_snapshots_or(&loop_flow->continue_states,
                                &loop_flow->has_continue_states,
                                state);
}

static void
destroy_loop_flow_state(LoopFlowState *loop_flow)
{
    if (loop_flow == NULL)
        return;
    destroy_resource_snapshot(&loop_flow->break_states);
    destroy_resource_snapshot(&loop_flow->continue_states);
    loop_flow->has_break_states = false;
    loop_flow->has_continue_states = false;
}

static FlowFlags
type_check_block_flow(ASTNode *node, SemanticContext *ctx,
                      LoopFlowState *loop_flow)
{
    if (node == NULL)
        return FLOW_FALLTHROUGH;

    if (node->type != AST_BLOCK)
        return type_check_statement_flow(node, ctx, loop_flow);

    FlowFlags flags = FLOW_FALLTHROUGH;
    for (size_t i = 0; i < node->data.block.count; i++) {
        if ((flags & FLOW_FALLTHROUGH) == 0)
            break;

        FlowFlags stmt_flags =
            type_check_statement_flow(node->data.block.statements[i], ctx, loop_flow);

        flags &= ~FLOW_FALLTHROUGH;
        flags |= (stmt_flags & (FLOW_FALLTHROUGH
                              | FLOW_BREAK
                              | FLOW_CONTINUE
                              | FLOW_RETURN));
    }

    return flags;
}

static FlowFlags
type_check_if_stmt_flow(ASTNode *node, SemanticContext *ctx,
                        LoopFlowState *loop_flow)
{
    Type *cond = type_check_expression(node->data.if_stmt.condition, ctx);
    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    FlowFlags flags = FLOW_NONE;
    FlowFlags then_flags = FLOW_NONE;

    if (!type_equals(cond, TYPE_BOOL)) {
        semantic_error(ctx, node,
            "If condition must be Bool, got '%s'", cond->name);
    }

    restore_resource_states(&base);
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    then_flags = type_check_block_flow(node->data.if_stmt.then_branch, ctx, loop_flow);
    scope_exit(&ctx->scope);
    flags |= (then_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
    if (then_flags & FLOW_FALLTHROUGH) {
        ResourceConsumeSnapshot then_snap = snapshot_resource_states(ctx);
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &then_snap);
        destroy_resource_snapshot(&then_snap);
        flags |= FLOW_FALLTHROUGH;
    }

    if (node->data.if_stmt.else_branch != NULL) {
        FlowFlags else_flags = FLOW_NONE;
        restore_resource_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        else_flags =
            type_check_statement_flow(node->data.if_stmt.else_branch, ctx, loop_flow);
        scope_exit(&ctx->scope);
        flags |= (else_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (else_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot else_snap = snapshot_resource_states(ctx);
            merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &else_snap);
            destroy_resource_snapshot(&else_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else {
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
    }

    if (has_fallthrough)
        restore_resource_states(&fallthrough);
    else
        restore_resource_states(&base);

    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&fallthrough);
    return flags;
}

static FlowFlags
type_check_match_stmt_flow(ASTNode *node, SemanticContext *ctx,
                           LoopFlowState *loop_flow)
{
    Type *subj_type = type_check_expression(node->data.match_stmt.subject, ctx);
    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    FlowFlags flags = FLOW_NONE;

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        bool handled = false;

        restore_resource_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);

        if (mc->data.match_case.pattern != NULL) {
            if (!type_check_special_match_pattern(
                    mc->data.match_case.pattern, subj_type, ctx, &handled)) {
                /* error already emitted */
            } else if (!handled) {
                Type *pat_type = type_check_expression(mc->data.match_case.pattern, ctx);
                if (!type_is_assignable(pat_type, subj_type) &&
                    !type_is_assignable(subj_type, pat_type)) {
                    semantic_error(ctx, mc->data.match_case.pattern,
                        "Case pattern type '%s' incompatible with match subject '%s'",
                        pat_type->name, subj_type->name);
                }
            }
        }

        if (mc->data.match_case.guard != NULL) {
            Type *guard_type = type_check_expression(mc->data.match_case.guard, ctx);
            if (!type_equals(guard_type, TYPE_BOOL)) {
                semantic_error(ctx, mc->data.match_case.guard,
                    "Case guard must be Bool, got '%s'", guard_type->name);
            }
        }

        FlowFlags case_flags =
            type_check_block_flow(mc->data.match_case.body, ctx, loop_flow);
        scope_exit(&ctx->scope);
        flags |= (case_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (case_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot case_snap = snapshot_resource_states(ctx);
            merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &case_snap);
            destroy_resource_snapshot(&case_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    }

    if (node->data.match_stmt.default_body != NULL) {
        FlowFlags default_flags = FLOW_NONE;
        restore_resource_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        default_flags =
            type_check_block_flow(node->data.match_stmt.default_body, ctx, loop_flow);
        scope_exit(&ctx->scope);
        flags |= (default_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (default_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot default_snap = snapshot_resource_states(ctx);
            merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &default_snap);
            destroy_resource_snapshot(&default_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else {
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
    }

    check_match_redundancy(node, subj_type, ctx);
    check_match_exhaustiveness(node, subj_type, ctx);

    if (has_fallthrough)
        restore_resource_states(&fallthrough);
    else
        restore_resource_states(&base);

    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&fallthrough);
    return flags;
}

static FlowFlags
type_check_with_stmt_flow(ASTNode *node, SemanticContext *ctx,
                          LoopFlowState *loop_flow)
{
    scope_enter(&ctx->scope, SCOPE_WITH);

    ASTNode *slot_type_node = node->data.with_stmt.slot_type;
    const char *alias = node->data.with_stmt.alias;
    bool is_secure = node->data.with_stmt.is_secure;

    Type *inner = resolve_type_node(slot_type_node, ctx);
    Type *slot_type = type_create_slot(inner, is_secure);

    Symbol *sym = symbol_create_slot(alias, slot_type, is_secure, NULL,
                                     node->line, node->column);
    scope_declare(ctx->scope, sym);
    scope_register_slot(ctx->scope, sym);

    FlowFlags flags = type_check_block_flow(node->data.with_stmt.body, ctx, loop_flow);

    scope_auto_release_slots(ctx->scope);
    scope_exit(&ctx->scope);
    return flags;
}

static FlowFlags
type_check_statement_flow(ASTNode *node, SemanticContext *ctx,
                          LoopFlowState *loop_flow)
{
    if (node == NULL)
        return FLOW_FALLTHROUGH;

    switch (node->type) {
    case AST_BLOCK:
        return type_check_block_flow(node, ctx, loop_flow);
    case AST_IF_STMT:
        return type_check_if_stmt_flow(node, ctx, loop_flow);
    case AST_MATCH_STMT:
        return type_check_match_stmt_flow(node, ctx, loop_flow);
    case AST_WITH_STMT:
        return type_check_with_stmt_flow(node, ctx, loop_flow);
    case AST_UNSAFE_BLOCK:
        if (node->data.unsafe_block.body != NULL)
            return type_check_block_flow(node->data.unsafe_block.body, ctx, loop_flow);
        return FLOW_FALLTHROUGH;
    case AST_DEFER_STMT:
        if (node->data.defer_stmt.body != NULL)
            return type_check_block_flow(node->data.defer_stmt.body, ctx, loop_flow);
        return FLOW_FALLTHROUGH;
    case AST_RETURN:
        type_check_return_stmt(node, ctx);
        return FLOW_RETURN;
    case AST_BREAK:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'break' used outside of loop");
            return FLOW_NONE;
        }
        {
            ResourceConsumeSnapshot snap = snapshot_resource_states_from_scope(
                loop_flow != NULL && loop_flow->loop_scope != NULL
                    ? loop_flow->loop_scope
                    : ctx->scope);
            loop_flow_record(loop_flow, true, &snap);
            destroy_resource_snapshot(&snap);
        }
        return FLOW_BREAK;
    case AST_CONTINUE:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'continue' used outside of loop");
            return FLOW_NONE;
        }
        {
            ResourceConsumeSnapshot snap = snapshot_resource_states_from_scope(
                loop_flow != NULL && loop_flow->loop_scope != NULL
                    ? loop_flow->loop_scope
                    : ctx->scope);
            loop_flow_record(loop_flow, false, &snap);
            destroy_resource_snapshot(&snap);
        }
        return FLOW_CONTINUE;
    default:
        type_check_statement(node, ctx);
        return FLOW_FALLTHROUGH;
    }
}

bool
type_check_block(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return true;

    (void)type_check_block_flow(node, ctx, NULL);
    return !ctx->has_error;
}

bool
type_check_if_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_if_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}

bool
type_check_for_loop(ASTNode *node, SemanticContext *ctx)
{
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    ctx->loop_depth++;

    /* Determine loop variable type:
     * - Range loop (start..end): variable is Int
     * - For-in loop (collection): variable is element type of collection */
    Type *var_type = TYPE_INT;
    if (node->data.for_loop.iterable != NULL) {
        Type *coll_type = type_check_expression(node->data.for_loop.iterable, ctx);
        if (type_is_constructed_named(coll_type, "Array")
            || type_is_constructed_named(coll_type, "Slice")
            || type_is_constructed_named(coll_type, "List")) {
            var_type = type_get_constructed_arg(coll_type, 0);
        } else if (coll_type != NULL && coll_type != TYPE_UNKNOWN) {
            semantic_error(ctx, node->data.for_loop.iterable,
                "for-in requires Array<T>, Slice<T>, or List<T>, got '%s'",
                coll_type->name != NULL ? coll_type->name : "<unknown>");
        }
    }

    Symbol *loop_var = symbol_create_variable(
        node->data.for_loop.variable, var_type, node->line, node->column);
    scope_declare(ctx->scope, loop_var);

    if (node->data.for_loop.range_start != NULL) {
        Type *t = type_check_expression(node->data.for_loop.range_start, ctx);
        require_assignable(t, TYPE_INT, node->data.for_loop.range_start, ctx);
    }
    if (node->data.for_loop.range_end != NULL) {
        Type *t = type_check_expression(node->data.for_loop.range_end, ctx);
        require_assignable(t, TYPE_INT, node->data.for_loop.range_end, ctx);
    }

    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot merged = copy_resource_snapshot(&base);
    ResourceConsumeSnapshot entry = copy_resource_snapshot(&base);
    bool known_iterations = false;
    size_t known_cap = for_loop_known_iteration_cap(node, &known_iterations);
    size_t max_iterations = (known_iterations && known_cap <= 1)
        ? 1
        : (base.count + 1);
    if (max_iterations == 0)
        max_iterations = 1;

    for (size_t iter = 0; iter < max_iterations; iter++) {
        LoopFlowState loop_flow = {0};
        ResourceConsumeSnapshot backedge = {0};
        bool has_backedge = false;
        FlowFlags body_flags = FLOW_NONE;

        loop_flow.loop_scope = ctx->scope;
        restore_resource_states(&entry);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        body_flags = type_check_block_flow(node->data.for_loop.body, ctx, &loop_flow);
        scope_exit(&ctx->scope);
        if (body_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot body_snap = snapshot_resource_states(ctx);
            merge_resource_states_or(&merged, &body_snap);
            merge_resource_snapshots_or(&backedge, &has_backedge, &body_snap);
            destroy_resource_snapshot(&body_snap);
        }

        if (loop_flow.has_continue_states)
            merge_resource_snapshots_or(&backedge, &has_backedge,
                                        &loop_flow.continue_states);
        if (loop_flow.has_break_states)
            merge_resource_states_or(&merged, &loop_flow.break_states);

        destroy_loop_flow_state(&loop_flow);

        if (!has_backedge) {
            destroy_resource_snapshot(&backedge);
            break;
        }

        if (resource_snapshots_equal(&entry, &backedge)) {
            destroy_resource_snapshot(&entry);
            entry = backedge;
            break;
        }

        destroy_resource_snapshot(&entry);
        entry = backedge;
    }

    ctx->loop_depth--;
    scope_exit(&ctx->scope);

    restore_resource_states(&merged);
    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&merged);
    destroy_resource_snapshot(&entry);
    return !ctx->has_error;
}

bool
type_check_while_loop(ASTNode *node, SemanticContext *ctx)
{
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    ctx->loop_depth++;

    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot merged = copy_resource_snapshot(&base);
    ResourceConsumeSnapshot entry = copy_resource_snapshot(&base);
    size_t max_iterations = base.count + 1;
    if (max_iterations == 0)
        max_iterations = 1;

    for (size_t iter = 0; iter < max_iterations; iter++) {
        LoopFlowState loop_flow = {0};
        ResourceConsumeSnapshot backedge = {0};
        bool has_backedge = false;
        FlowFlags body_flags = FLOW_NONE;

        loop_flow.loop_scope = ctx->scope;
        restore_resource_states(&entry);
        Type *cond = type_check_expression(node->data.while_loop.condition, ctx);
        if (!type_equals(cond, TYPE_BOOL)) {
            semantic_error(ctx, node,
                "While condition must be Bool, got '%s'", cond->name);
        }

        scope_enter(&ctx->scope, SCOPE_BLOCK);
        body_flags = type_check_block_flow(node->data.while_loop.body, ctx, &loop_flow);
        scope_exit(&ctx->scope);
        if (body_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot body_snap = snapshot_resource_states(ctx);
            merge_resource_states_or(&merged, &body_snap);
            merge_resource_snapshots_or(&backedge, &has_backedge, &body_snap);
            destroy_resource_snapshot(&body_snap);
        }

        if (loop_flow.has_continue_states)
            merge_resource_snapshots_or(&backedge, &has_backedge,
                                        &loop_flow.continue_states);
        if (loop_flow.has_break_states)
            merge_resource_states_or(&merged, &loop_flow.break_states);

        destroy_loop_flow_state(&loop_flow);

        if (!has_backedge) {
            destroy_resource_snapshot(&backedge);
            break;
        }

        if (resource_snapshots_equal(&entry, &backedge)) {
            destroy_resource_snapshot(&entry);
            entry = backedge;
            break;
        }

        destroy_resource_snapshot(&entry);
        entry = backedge;
    }

    ctx->loop_depth--;
    scope_exit(&ctx->scope);

    restore_resource_states(&merged);
    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&merged);
    destroy_resource_snapshot(&entry);
    return !ctx->has_error;
}

bool
type_check_match_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_match_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}

bool
type_check_with_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_with_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}
