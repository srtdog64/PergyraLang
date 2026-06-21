/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * Domain-lifecycle analysis pass - see lifecycle_analyze.h and
 * docs/semantics/12_domain_lifecycle_evidence.md.
 *
 * Collects parser-owned `lifecycle <S> { Op: From -> To; }` AST declarations
 * into the lifecycle registry, then tracks each governed local's state through a
 * function body, static-rejecting an op whose precondition the statically-known
 * state violates. Control-flow joins merge through lc_merge, so the lifecycle
 * engine remains the single source of truth for "known", "ambiguous", and
 * "needs runtime evidence" verdicts.
 */

#include "lifecycle_analyze.h"
#include "lifecycle_state.h"
#include "../parser/ast_api.h"
#include "../parser/ast.h"
#include "type_checker.h"
#include "type_checker_resolution_internal.h"

#include <string.h>

#define LC_WALK_MAX_VARS 128
#define LC_ACC_MAX_OPS   256
#define LC_ACC_MAX_LETS  128

typedef struct {
    char          name[LC_NAME_LEN];
    const LcSpec *spec;
    LcState       state;
} LcVarState;

/* One recorded lifecycle operation site (a `v.Op()` call). Accumulated across
 * the whole function -- independent of the per-branch LcWalk copies -- so a
 * post-pass can decide which variables need runtime tracking (taint) and then
 * annotate every op-site of a tainted variable. */
typedef struct {
    char        var[LC_NAME_LEN];
    const void *node;       /* AST_CALL node (guard key) */
    int         verdict;    /* LcResult at this site */
    uint32_t    valid_mask; /* permitted-from set (for LC_NEEDS_RUNTIME_CHECK) */
    int         to_state;   /* deterministic post-state, or -1 if ambiguous */
    char        op[LC_NAME_LEN];
    char        subject[LC_NAME_LEN];
} LcOpRecord;

/* One governed `let` site -- the construction point where a tracked variable's
 * runtime state tag is initialised. */
typedef struct {
    char        var[LC_NAME_LEN];
    const void *node;        /* AST_LET_DECL node (guard key) */
    int         init_state;
    char        subject[LC_NAME_LEN];
} LcLetRecord;

/* Per-function accumulator. Shared by pointer from every (copied) LcWalk so that
 * op-sites recorded on either side of a branch all land here exactly once. */
typedef struct {
    LcOpRecord  ops[LC_ACC_MAX_OPS];
    int         op_count;
    LcLetRecord lets[LC_ACC_MAX_LETS];
    int         let_count;
} LcFnAccum;

typedef struct {
    LcVarState       vars[LC_WALK_MAX_VARS];
    int              count;
    SemanticContext *ctx;
    char             scope_prefix[LC_NAME_LEN];
    LcFnAccum       *acc;   /* shared; NOT deep-copied across branches */
} LcWalk;

static void
lc_copy_var_name(char *dst, const char *src)
{
    size_t i = 0;
    if (src != NULL)
        for (; src[i] != '\0' && i + 1 < LC_NAME_LEN; i++)
            dst[i] = src[i];
    dst[i] = '\0';
}

static bool
lc_copy_name_exact(char *dst, const char *src)
{
    size_t len;

    if (dst == NULL || src == NULL)
        return false;
    len = strlen(src);
    if (len >= LC_NAME_LEN)
        return false;
    memcpy(dst, src, len + 1);
    return true;
}

static bool
lc_make_scoped_name(char out[LC_NAME_LEN], const char *prefix,
                    const char *name)
{
    size_t plen = prefix != NULL ? strlen(prefix) : 0;
    size_t nlen = name != NULL ? strlen(name) : 0;

    if (out == NULL || name == NULL || name[0] == '\0')
        return false;
    if (plen + nlen >= LC_NAME_LEN)
        return false;
    if (plen > 0)
        memcpy(out, prefix, plen);
    memcpy(out + plen, name, nlen + 1);
    return true;
}

static bool
lc_make_namespace_prefix(char out[LC_NAME_LEN], const char *parent,
                         const char *namespace_name)
{
    size_t plen = parent != NULL ? strlen(parent) : 0;
    size_t nlen = namespace_name != NULL ? strlen(namespace_name) : 0;

    if (out == NULL || namespace_name == NULL || namespace_name[0] == '\0')
        return false;
    if (plen + nlen + 1 >= LC_NAME_LEN)
        return false;
    if (plen > 0)
        memcpy(out, parent, plen);
    memcpy(out + plen, namespace_name, nlen);
    out[plen + nlen] = '_';
    out[plen + nlen + 1] = '\0';
    return true;
}

static LcVarState *
lc_walk_find(LcWalk *w, const char *name)
{
    if (name == NULL)
        return NULL;
    for (int i = 0; i < w->count; i++) {
        if (strcmp(w->vars[i].name, name) == 0)
            return &w->vars[i];
    }
    return NULL;
}

static void
lc_walk_mark_all_ambiguous(LcWalk *w)
{
    for (int i = 0; i < w->count; i++)
        w->vars[i].state = LC_AMBIGUOUS;
}

static LcState
lc_walk_state_for(const LcWalk *w, const char *name, LcState fallback)
{
    if (w == NULL || name == NULL)
        return fallback;
    for (int i = 0; i < w->count; i++) {
        if (strcmp(w->vars[i].name, name) == 0)
            return w->vars[i].state;
    }
    return fallback;
}

static void
lc_walk_merge_existing_vars(LcWalk *dst, const LcWalk *left,
                            const LcWalk *right)
{
    for (int i = 0; i < dst->count; i++) {
        LcState original = dst->vars[i].state;
        LcState l = lc_walk_state_for(left, dst->vars[i].name, original);
        LcState r = lc_walk_state_for(right, dst->vars[i].name, original);
        dst->vars[i].state = lc_merge(l, r);
    }
}

static bool
lc_walk_lifecycle_subject_name(LcWalk *w, ASTNode *type_ref,
                               char out[LC_NAME_LEN])
{
    Type *resolved;
    const char *name;

    if (w == NULL || type_ref == NULL)
        return false;

    resolved = semantic_type_resolution_lookup_metadata_type_ref(w->ctx,
                                                                 type_ref);
    name = resolved != NULL ? resolved->name : NULL;
    if (name != NULL && lc_copy_name_exact(out, name))
        return true;

    name = ast_type_name(type_ref);
    return lc_make_scoped_name(out, w->scope_prefix, name);
}

/* Recognise a statement of the form `v.Op()` (a member call on a bare
 * identifier). Returns the receiver var name and the method name. */
static bool
lc_extract_method_call(ASTNode *stmt, const char **var_out, const char **op_out)
{
    ASTNode *callee;
    ASTNode *object;

    if (stmt == NULL || stmt->type != AST_CALL)
        return false;
    callee = ast_call_callee(stmt);
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
        return false;
    object = ast_member_object(callee);
    if (object == NULL || object->type != AST_IDENTIFIER)
        return false;
    *var_out = ast_identifier_name(object);
    *op_out = ast_member_name(callee);
    return *var_out != NULL && *op_out != NULL;
}

static void
lc_acc_record_let(LcWalk *w, const char *var, const void *node,
                  int init_state, const char *subject)
{
    LcLetRecord *rec;
    if (w->acc == NULL || w->acc->let_count >= LC_ACC_MAX_LETS)
        return;
    rec = &w->acc->lets[w->acc->let_count++];
    lc_copy_var_name(rec->var, var);
    rec->node = node;
    rec->init_state = init_state;
    lc_copy_var_name(rec->subject, subject);
}

static void
lc_acc_record_op(LcWalk *w, const char *var, const void *node, int verdict,
                 uint32_t valid_mask, int to_state, const char *op,
                 const char *subject)
{
    LcOpRecord *rec;
    if (w->acc == NULL || w->acc->op_count >= LC_ACC_MAX_OPS)
        return;
    rec = &w->acc->ops[w->acc->op_count++];
    lc_copy_var_name(rec->var, var);
    rec->node = node;
    rec->verdict = verdict;
    rec->valid_mask = valid_mask;
    rec->to_state = to_state;
    lc_copy_var_name(rec->op, op);
    lc_copy_var_name(rec->subject, subject);
}

static void lc_walk_block(LcWalk *w, ASTNode *block);

static void
lc_walk_stmt(LcWalk *w, ASTNode *stmt)
{
    const char *var_name = NULL;
    const char *op_name = NULL;

    if (stmt == NULL)
        return;

    /* `let v: <Subject> = ...` for a governed Subject -> start in the initial
     * (first declared) state. */
    if (stmt->type == AST_LET_DECL) {
        const char *vname = ast_let_name(stmt);
        ASTNode    *tnode = ast_let_type(stmt);
        char scoped_type[LC_NAME_LEN];
        const LcSpec *spec = NULL;
        if (lc_walk_lifecycle_subject_name(w, tnode, scoped_type))
            spec = lc_registry_find(scoped_type);
        if (spec != NULL && vname != NULL && w->count < LC_WALK_MAX_VARS) {
            LcVarState *v = &w->vars[w->count++];
            lc_copy_var_name(v->name, vname);
            v->spec = spec;
            v->state = (spec->state_count > 0) ? 0 : LC_UNINIT;
            /* Record the construction site; the post-pass decides whether this
             * variable is runtime-tracked (taint) and only then emits an init. */
            if (v->state >= 0)
                lc_acc_record_let(w, vname, stmt, v->state, spec->subject);
        }
        return;
    }

    /* `v.Op()` -> apply the transition; static-reject a precondition violation. */
    if (lc_extract_method_call(stmt, &var_name, &op_name)) {
        LcVarState *v = lc_walk_find(w, var_name);
        if (v != NULL) {
            int op_idx = lc_spec_op_index(v->spec, op_name);
            if (op_idx >= 0) { /* a declared lifecycle op, not an ordinary method */
                LcMachine m = lc_spec_machine(v->spec);
                LcState    next;
                LcResult   r = lc_apply_op(&m, v->state, op_idx, &next);
                if (r == LC_ERR_PRECONDITION) {
                    /* Statically-known state forbids the op: fail closed at
                     * compile time (zero runtime cost, no false positive). */
                    semantic_error(w->ctx, stmt,
                        "Lifecycle violation: '%s' on '%s' is not permitted in "
                        "state '%s'", op_name, var_name,
                        lc_state_name(&m, v->state));
                } else if (r == LC_OK || r == LC_NEEDS_RUNTIME_CHECK) {
                    /* OK: a proven transition (recorded so a later ambiguous
                     * guard on this variable sees the right state).
                     * NEEDS_RUNTIME_CHECK: state ambiguous -> fail-closed runtime
                     * guard. The post-pass turns these into guard annotations,
                     * but only for variables that are actually runtime-tracked. */
                    int rec_to = (next >= 0) ? (int)next : -1;
                    lc_acc_record_op(w, var_name, stmt, (int)r,
                        lc_op_valid_from_mask(&m, op_idx), rec_to,
                        op_name, v->spec->subject);
                }
                v->state = next;
            }
        }
        return;
    }

    if (stmt->type == AST_BLOCK) {
        lc_walk_block(w, stmt);
        return;
    }

    if (stmt->type == AST_IF_STMT) {
        LcWalk then_walk = *w;
        LcWalk else_walk = *w;
        lc_walk_stmt(&then_walk, ast_if_then_branch(stmt));
        if (ast_if_else_branch(stmt) != NULL)
            lc_walk_stmt(&else_walk, ast_if_else_branch(stmt));
        lc_walk_merge_existing_vars(w, &then_walk, &else_walk);
        return;
    }

    if (stmt->type == AST_WHILE_LOOP || stmt->type == AST_FOR_LOOP) {
        LcWalk before_walk = *w;
        LcWalk body_walk = *w;
        lc_walk_stmt(&body_walk, stmt->type == AST_WHILE_LOOP
            ? ast_while_body(stmt)
            : ast_for_body(stmt));
        lc_walk_merge_existing_vars(w, &before_walk, &body_walk);
        return;
    }

    if (stmt->type == AST_MATCH_STMT) {
        LcWalk merged_walk = *w;
        bool seen_branch = false;
        for (size_t i = 0; i < ast_match_case_count(stmt); i++) {
            ASTNode *match_case = ast_match_case_at(stmt, i);
            LcWalk case_walk = *w;
            lc_walk_stmt(&case_walk, ast_match_case_body(match_case));
            if (!seen_branch) {
                merged_walk = case_walk;
                seen_branch = true;
            } else {
                LcWalk before_merge = merged_walk;
                lc_walk_merge_existing_vars(&merged_walk, &before_merge,
                                            &case_walk);
            }
        }
        if (ast_match_default_body(stmt) != NULL) {
            LcWalk default_walk = *w;
            lc_walk_stmt(&default_walk, ast_match_default_body(stmt));
            if (!seen_branch) {
                merged_walk = default_walk;
                seen_branch = true;
            } else {
                LcWalk before_merge = merged_walk;
                lc_walk_merge_existing_vars(&merged_walk, &before_merge,
                                            &default_walk);
            }
        } else if (seen_branch) {
            LcWalk before_merge = merged_walk;
            lc_walk_merge_existing_vars(&merged_walk, &before_merge, w);
        }
        if (seen_branch)
            lc_walk_merge_existing_vars(w, &merged_walk, &merged_walk);
        else
            lc_walk_mark_all_ambiguous(w);
        return;
    }
}

static void
lc_walk_block(LcWalk *w, ASTNode *block)
{
    size_t n;
    if (block == NULL)
        return;
    n = ast_block_statement_count(block);
    for (size_t i = 0; i < n; i++)
        lc_walk_stmt(w, ast_block_statement(block, i));
}

static bool
lc_register_declaration(ASTNode *decl, SemanticContext *ctx,
                        const char *scope_prefix)
{
    const char *subject;
    char scoped_subject[LC_NAME_LEN];
    int sid;

    if (decl == NULL || decl->type != AST_LIFECYCLE_DECL)
        return true;

    subject = ast_lifecycle_subject(decl);
    if (!lc_make_scoped_name(scoped_subject, scope_prefix, subject)) {
        semantic_error(ctx, decl,
            "Invalid lifecycle subject name '%s'",
            subject != NULL ? subject : "<unknown>");
        return false;
    }
    sid = lc_registry_begin(scoped_subject);
    if (sid < 0) {
        semantic_error(ctx, decl,
            "Duplicate or invalid lifecycle declaration for '%s'",
            scoped_subject);
        return false;
    }

    for (size_t i = 0; i < ast_lifecycle_transition_count(decl); i++) {
        const LifecycleTransitionDecl *t = ast_lifecycle_transition(decl, i);
        if (t == NULL
            || !lc_registry_add_transition(sid, t->op, t->from_state,
                                           t->to_state)) {
            semantic_error(ctx, decl,
                "Conflicting or invalid lifecycle transition '%s: %s -> %s'",
                t != NULL && t->op != NULL ? t->op : "<op>",
                t != NULL && t->from_state != NULL ? t->from_state : "<from>",
                t != NULL && t->to_state != NULL ? t->to_state : "<to>");
            return false;
        }
    }
    return true;
}

static bool
lc_collect_declarations(ASTNode *node, SemanticContext *ctx,
                        const char *scope_prefix)
{
    if (node == NULL)
        return true;

    if (node->type == AST_LIFECYCLE_DECL)
        return lc_register_declaration(node, ctx, scope_prefix);

    if (node->type == AST_PROGRAM) {
        for (size_t i = 0; i < ast_program_statement_count(node); i++) {
            if (!lc_collect_declarations(ast_program_statement(node, i), ctx,
                                         scope_prefix))
                return false;
        }
        return true;
    }

    if (node->type == AST_NAMESPACE_DECL) {
        char child_prefix[LC_NAME_LEN];
        if (!lc_make_namespace_prefix(child_prefix, scope_prefix,
                                      ast_namespace_name(node))) {
            semantic_error(ctx, node,
                "Invalid lifecycle namespace name '%s'",
                ast_namespace_name(node) != NULL
                    ? ast_namespace_name(node)
                    : "<unknown>");
            return false;
        }
        for (size_t i = 0; i < ast_namespace_statement_count(node); i++) {
            if (!lc_collect_declarations(ast_namespace_statement(node, i), ctx,
                                         child_prefix))
                return false;
        }
    }
    return true;
}

/* True iff some op-site for `var` was ambiguous (needs the runtime tag). Only
 * such variables are instrumented; fully statically-proven ones stay zero-cost. */
static bool
lc_var_is_runtime_tracked(const LcFnAccum *acc, const char *var)
{
    for (int i = 0; i < acc->op_count; i++) {
        if (acc->ops[i].verdict == LC_NEEDS_RUNTIME_CHECK
            && strcmp(acc->ops[i].var, var) == 0)
            return true;
    }
    return false;
}

/* Turn the accumulated op/let sites into runtime-guard annotations. A variable
 * is instrumented only if it has at least one ambiguous op; then its
 * construction is annotated with an init (LC_GUARD_SET) and each of its op-sites
 * with a SET (proven transition, keeps the tag current) or a CHECK (the
 * fail-closed ambiguous guard). */
static void
lc_annotate_runtime_guards(const LcFnAccum *acc)
{
    for (int i = 0; i < acc->let_count; i++) {
        const LcLetRecord *L = &acc->lets[i];
        if (!lc_var_is_runtime_tracked(acc, L->var))
            continue;
        lc_guard_add(L->node, LC_GUARD_SET, 0, L->init_state, NULL, L->subject);
    }
    for (int i = 0; i < acc->op_count; i++) {
        const LcOpRecord *O = &acc->ops[i];
        if (!lc_var_is_runtime_tracked(acc, O->var))
            continue;
        lc_guard_add(O->node,
            O->verdict == LC_NEEDS_RUNTIME_CHECK ? LC_GUARD_CHECK : LC_GUARD_SET,
            O->valid_mask, O->to_state, O->op, O->subject);
    }
}

static void
lc_analyze_function_declaration(ASTNode *decl, SemanticContext *ctx,
                                const char *scope_prefix)
{
    ASTNode  *body;
    LcWalk    w;
    LcFnAccum accum;

    if (decl == NULL || decl->type != AST_FUNC_DECL)
        return;
    body = ast_func_body(decl);
    if (body == NULL)
        return;
    memset(&w, 0, sizeof(w));
    memset(&accum, 0, sizeof(accum));
    w.ctx = ctx;
    w.acc = &accum;
    if (!lc_copy_name_exact(w.scope_prefix,
                            scope_prefix != NULL ? scope_prefix : "")) {
        semantic_error(ctx, decl,
            "Invalid lifecycle namespace scope while analyzing function");
        return;
    }
    lc_walk_block(&w, body);
    lc_annotate_runtime_guards(&accum);
}

static void
lc_analyze_function_declarations(ASTNode *node, SemanticContext *ctx,
                                 const char *scope_prefix)
{
    if (node == NULL)
        return;

    if (node->type == AST_FUNC_DECL) {
        lc_analyze_function_declaration(node, ctx, scope_prefix);
        return;
    }

    if (node->type == AST_PROGRAM) {
        for (size_t i = 0; i < ast_program_statement_count(node); i++)
            lc_analyze_function_declarations(ast_program_statement(node, i),
                                             ctx, scope_prefix);
        return;
    }

    if (node->type == AST_NAMESPACE_DECL) {
        char child_prefix[LC_NAME_LEN];
        if (!lc_make_namespace_prefix(child_prefix, scope_prefix,
                                      ast_namespace_name(node))) {
            semantic_error(ctx, node,
                "Invalid lifecycle namespace name '%s'",
                ast_namespace_name(node) != NULL
                    ? ast_namespace_name(node)
                    : "<unknown>");
            return;
        }
        for (size_t i = 0; i < ast_namespace_statement_count(node); i++)
            lc_analyze_function_declarations(ast_namespace_statement(node, i),
                                             ctx, child_prefix);
    }
}

bool
lifecycle_analyze_program(ASTNode *program, SemanticContext *ctx)
{
    if (program == NULL)
        return true;
    lc_registry_reset();
    lc_guard_reset();
    if (!lc_collect_declarations(program, ctx, ""))
        return false;
    /* No lifecycle declarations -> nothing to enforce (no false positives on
     * lifecycle-free programs). */
    if (lc_registry_count() == 0)
        return true;

    lc_analyze_function_declarations(program, ctx, "");
    return true;
}
