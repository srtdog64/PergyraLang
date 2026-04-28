#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../common/string_compat.h"
#include "type_checker_internal.h"
#include "diag_codes.h"

static Type *
program_resolve_type_quiet(ASTNode *type_node, SemanticContext *ctx)
{
    Type *resolved;

    if (type_node == NULL || ctx == NULL)
        return TYPE_UNKNOWN;

    resolved = semantic_type_resolution_lookup_resolved_type(ctx, type_node);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static Type *
program_resolve_func_return_type_quiet(ASTNode *func_decl, SemanticContext *ctx)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.return_type == NULL) {
        return TYPE_VOID;
    }
    return program_resolve_type_quiet(func_decl->data.func_decl.return_type, ctx);
}

static Type *
program_resolve_intent_binding_type_quiet(ASTNode *binding, SemanticContext *ctx)
{
    if (binding == NULL)
        return TYPE_UNKNOWN;
    if (binding->type == AST_INTENT_INVOLVES)
        return program_resolve_type_quiet(
            binding->data.intent_involves.subject_type, ctx);
    if (binding->type == AST_INTENT_VALUE)
        return program_resolve_type_quiet(
            binding->data.intent_value.value_type, ctx);
    return TYPE_UNKNOWN;
}

bool
type_check_program(ASTNode *program, SemanticContext *ctx)
{
    size_t *topo_order = NULL;
    size_t topo_count = 0;

    if (program == NULL || program->type != AST_PROGRAM)
        return false;

    ctx->program_root = program;
    semantic_type_resolution_precollect_program(program, ctx);

    /*
     * Pass 1: collect all top-level function and class names
     * so that forward references within the same file work.
     */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt->type == AST_TYPE_ALIAS) {
            const char *tname = stmt->data.type_alias.name;
            if (tname != NULL && scope_lookup_current(ctx->scope, tname) == NULL) {
                Symbol *s = symbol_create_function(tname, TYPE_UNKNOWN,
                    stmt->line, stmt->column);
                if (s != NULL)
                    s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_CLASS_DECL) {
            const char *cname = stmt->data.class_decl.name;
            if (cname != NULL && scope_lookup_current(ctx->scope, cname) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_CLASS;
                    t->nominal_flavor = nominal_flavor_from_decl(stmt);
                    t->name = pergyra_strdup(cname);
                }
                Symbol *s = symbol_create_function(cname,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                if (s != NULL)
                    s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_ABILITY_DECL) {
            const char *aname = stmt->data.ability_decl.name;
            if (aname != NULL && scope_lookup_current(ctx->scope, aname) == NULL) {
                Symbol *s = symbol_create_function(aname, TYPE_VOID,
                                                    stmt->line, stmt->column);
                if (s != NULL)
                    s->kind = SYMBOL_ABILITY;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_FUNC_DECL) {
            const char *fname = stmt->data.func_decl.name;
            if (scope_lookup_current(ctx->scope, fname) == NULL) {
                /* Forward-declare with correct param count so that
                 * call-site arity checks pass before Pass 2. */
                size_t fpc = stmt->data.func_decl.param_count;
                /* Exclude implicit 'self' param from count */
                size_t real_pc = 0;
                for (size_t j = 0; j < fpc; j++) {
                    FuncParam *p = stmt->data.func_decl.params[j];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    real_pc++;
                }
                Type **ptypes = calloc(real_pc > 0 ? real_pc : 1,
                                         sizeof(Type *));
                for (size_t j = 0; j < real_pc; j++)
                    ptypes[j] = TYPE_UNKNOWN;
                Type *ret = program_resolve_func_return_type_quiet(stmt, ctx);
                Type *placeholder = type_create_function(ptypes, real_pc, ret);
                if (placeholder != NULL)
                    placeholder->data.function.effect_mask =
                        declared_effects_from_function_node(stmt, ctx, NULL);
                free(ptypes);
                Symbol *s = symbol_create_function(fname, placeholder,
                                                    stmt->line, stmt->column);
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_EVENT_DECL) {
            const char *ename = stmt->data.event_decl.name;
            if (scope_lookup_current(ctx->scope, ename) == NULL) {
                size_t epc = stmt->data.event_decl.param_count;
                Type **eptypes = calloc(epc > 0 ? epc : 1, sizeof(Type *));
                for (size_t j = 0; j < epc; j++) {
                    ASTNode *p = stmt->data.event_decl.params[j];
                    if (p != NULL
                        && p->type == AST_LET_DECL
                        && p->data.let_decl.type != NULL) {
                        eptypes[j] = program_resolve_type_quiet(
                            p->data.let_decl.type, ctx);
                    } else {
                        eptypes[j] = TYPE_UNKNOWN;
                    }
                }
                Type *evt_ft = type_create_function(eptypes, epc, TYPE_VOID);
                free(eptypes);
                Symbol *s = symbol_create_function(ename, evt_ft,
                                                    stmt->line, stmt->column);
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_ENUM_DECL) {
            const char *ename = stmt->data.enum_decl.name;
            if (ename != NULL && scope_lookup_current(ctx->scope, ename) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_ENUM;
                    t->name = pergyra_strdup(ename);
                }
                Symbol *s = symbol_create_function(ename,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
            Symbol *enum_sym = scope_lookup_current(ctx->scope, ename);
            Type *etype = enum_sym != NULL ? enum_sym->type : TYPE_UNKNOWN;
            for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
                const char *vname = stmt->data.enum_decl.variants[j];
                if (vname == NULL || scope_lookup_current(ctx->scope, vname) != NULL)
                    continue;
                size_t vpc = (stmt->data.enum_decl.variant_param_counts != NULL)
                    ? stmt->data.enum_decl.variant_param_counts[j] : 0;
                if (vpc > 0) {
                    /* Tagged union variant constructor: register as function
                     * Circle(Int) -> Shape */
                    Type **ptypes = calloc(vpc, sizeof(Type *));
                    for (size_t p = 0; p < vpc && ptypes != NULL; p++) {
                        ASTNode *pt = stmt->data.enum_decl.variant_params[j][p];
                        ptypes[p] = program_resolve_type_quiet(pt, ctx);
                    }
                    Type *ft = type_create_function(ptypes, vpc, etype);
                    free(ptypes);
                    Symbol *vs = symbol_create_function(vname, ft,
                        stmt->line, stmt->column);
                    scope_declare(ctx->scope, vs);
                } else {
                    /* Simple variant: register as variable */
                    Symbol *vs = symbol_create_variable(vname, etype,
                        stmt->line, stmt->column);
                    scope_declare(ctx->scope, vs);
                }
            }
        } else if (stmt->type == AST_EXTERN_BLOCK) {
            for (size_t j = 0; j < stmt->data.extern_block.count; j++) {
                ASTNode *decl = stmt->data.extern_block.declarations[j];
                if (decl == NULL || decl->type != AST_FUNC_DECL)
                    continue;
                const char *fname = decl->data.func_decl.name;
                if (scope_lookup_current(ctx->scope, fname) == NULL) {
                    Type *placeholder = type_create_function(NULL, 0, TYPE_VOID);
                    if (placeholder != NULL)
                        placeholder->data.function.effect_mask =
                            declared_effects_from_function_node(decl, ctx, NULL);
                    Symbol *s = symbol_create_function(fname, placeholder,
                                                        decl->line, decl->column);
                    scope_declare(ctx->scope, s);
                }
            }
        } else if (stmt->type == AST_INTENT_DECL) {
            const char *iname = stmt->data.intent_decl.name;
            if (iname != NULL && scope_lookup_current(ctx->scope, iname) == NULL) {
                size_t ipc = stmt->data.intent_decl.binding_count > 0
                    ? stmt->data.intent_decl.binding_count
                    : (stmt->data.intent_decl.involve_count
                        + stmt->data.intent_decl.value_count);
                Type **ptypes = calloc(ipc > 0 ? ipc : 1, sizeof(Type *));
                for (size_t j = 0; j < ipc; j++) {
                    ASTNode *binding = stmt->data.intent_decl.binding_count > 0
                        ? stmt->data.intent_decl.bindings[j]
                        : (j < stmt->data.intent_decl.involve_count
                            ? stmt->data.intent_decl.involves[j]
                            : stmt->data.intent_decl.values[j - stmt->data.intent_decl.involve_count]);
                    if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                        && binding->data.intent_involves.subject_type != NULL) {
                        ptypes[j] = program_resolve_intent_binding_type_quiet(
                            binding, ctx);
                    } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                        && binding->data.intent_value.value_type != NULL) {
                        ptypes[j] = program_resolve_intent_binding_type_quiet(
                            binding, ctx);
                    } else {
                        ptypes[j] = TYPE_UNKNOWN;
                    }
                }
                Type *ft = type_create_function(ptypes, ipc, TYPE_BOOL);
                free(ptypes);
                Symbol *s = symbol_create_function(iname,
                    ft != NULL ? ft : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_INTENT;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_PARTY_DECL
                   || stmt->type == AST_ROSTER_DECL
                   || stmt->type == AST_WORLD_DECL
                   || stmt->type == AST_RELATION_DECL
                   || stmt->type == AST_EFFECT_DECL
                   || stmt->type == AST_ZONE_DECL) {
            /* Register domain declarations as class-like symbols
             * so that constructor-like syntax can be introduced consistently */
            const char *dname = NULL;
            if (stmt->type == AST_PARTY_DECL)
                dname = stmt->data.party_decl.name;
            else if (stmt->type == AST_ROSTER_DECL)
                dname = stmt->data.roster_decl.name;
            else if (stmt->type == AST_WORLD_DECL)
                dname = stmt->data.world_decl.name;
            else if (stmt->type == AST_RELATION_DECL)
                dname = stmt->data.relation_decl.name;
            else if (stmt->type == AST_EFFECT_DECL)
                dname = stmt->data.effect_decl.name;
            else
                dname = stmt->data.zone_decl.name;
            if (dname != NULL && scope_lookup_current(ctx->scope, dname) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_CLASS;
                    t->nominal_flavor = TYPE_NOMINAL_CLASS;
                    t->name = pergyra_strdup(dname);
                }
                Symbol *s = symbol_create_function(dname,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        }
    }

    if (!type_resolution_validate_graph(ctx))
        return false;
    if (!type_resolution_build_topo_order(&ctx->type_resolution_graph,
                                          &topo_order,
                                          &topo_count)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_DEPENDENCY_CYCLE, PGY_CAUSE_TYPE_RESOLUTION_CYCLE, PGY_FIX_BREAK_CYCLE_VIA_INDIRECTION, program,
            "Type resolution topological ordering could not be constructed.\n"
            "Reason:\n"
            "- the semantic type dependency graph is not acyclic or not fully materialized\n"
            "- graph-backed staged resolution cannot trust the current declaration order\n"
            "Fix:\n"
            "- resolve the earlier type dependency cycle\n"
            "- or close the missing generic/alias/ability dependency edge");
        free(topo_order);
        return false;
    }

    semantic_run_type_resolution_worklist(program, ctx, topo_order, topo_count);

    /*
     * Pass 2: full type-check
     */
    for (size_t i = 0; i < program->data.program.count; i++)
        type_check_statement(program->data.program.statements[i], ctx);

    (void)type_resolution_validate_graph(ctx);

    /* Optional instrumentation for type-resolution audit (단계 1.0).
     * Enabled when PGY_TYPE_RES_STATS is set. */
    {
        const char *stats_env = getenv("PGY_TYPE_RES_STATS");
        if (stats_env != NULL && stats_env[0] != '\0' && stats_env[0] != '0') {
            TypeResolutionGraph *g = &ctx->type_resolution_graph;
            size_t kind_counts[7] = {0};
            size_t *indeg = NULL;
            size_t *outdeg = NULL;
            size_t name_dup = 0;
            size_t topo_count = 0;
            size_t *topo = NULL;
            bool topo_ok;

            for (size_t i = 0; i < g->node_count; i++) {
                int k = (int)g->nodes[i].kind;
                if (k >= 0 && k < 7) kind_counts[k]++;
            }
            indeg = calloc(g->node_count > 0 ? g->node_count : 1, sizeof(size_t));
            outdeg = calloc(g->node_count > 0 ? g->node_count : 1, sizeof(size_t));
            if (indeg != NULL && outdeg != NULL) {
                for (size_t e = 0; e < g->edge_count; e++) {
                    if (g->edges[e].from < g->node_count) outdeg[g->edges[e].from]++;
                    if (g->edges[e].to < g->node_count)   indeg[g->edges[e].to]++;
                }
            }
            /* Detect duplicate labels (re-visits of same named type) */
            for (size_t i = 0; i < g->node_count; i++) {
                const char *li = g->nodes[i].label;
                if (li == NULL) continue;
                for (size_t j = i + 1; j < g->node_count; j++) {
                    const char *lj = g->nodes[j].label;
                    if (lj != NULL && strcmp(li, lj) == 0) { name_dup++; break; }
                }
            }
            if (topo_order != NULL) {
                topo_ok = true;
                topo = NULL;
                topo_count = g->node_count;
            } else {
                topo_ok = type_resolution_build_topo_order(g, &topo, &topo_count);
            }

            fprintf(stderr, "[type-res-stats] nodes=%llu edges=%llu duplicate_labels=%llu topo_ok=%d topo_produced=%llu/%llu\n",
                    (unsigned long long) g->node_count, (unsigned long long) g->edge_count, (unsigned long long) name_dup,
                    topo_ok ? 1 : 0, (unsigned long long) topo_count, (unsigned long long) g->node_count);
            fprintf(stderr, "[type-res-stats] resolve_type_node: calls=%llu unique_nodes=%llu revisit_rate=%.1f%%\n",
                    (unsigned long long) g_resolve_type_node_calls, (unsigned long long) g_resolve_type_node_unique_nodes,
                    g_resolve_type_node_calls > 0
                        ? 100.0 * (double)(g_resolve_type_node_calls - g_resolve_type_node_unique_nodes)
                          / (double)g_resolve_type_node_calls
                        : 0.0);
            fprintf(stderr, "[type-res-stats] stage-legacy-resolve: calls=%llu failed=%llu suppressed_diagnostics=%llu\n",
                    (unsigned long long) ctx->type_resolution_stage_legacy_resolve_count,
                    (unsigned long long) ctx->type_resolution_stage_legacy_resolve_failed_count,
                    (unsigned long long) ctx->type_resolution_stage_legacy_resolve_suppressed_diag_count);
            fprintf(stderr, "[type-res-stats] stage-graph-backed: skips=%llu\n",
                    (unsigned long long) ctx->type_resolution_stage_graph_backed_skip_count);
            size_t metadata_owned_count = 0;
            for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++) {
                if (ctx->type_resolution_metadata.owned[i])
                    metadata_owned_count++;
            }
            fprintf(stderr, "[type-res-stats] metadata: entries=%llu owned=%llu hits=%llu misses=%llu materializer_fallbacks=%llu\n",
                    (unsigned long long) ctx->type_resolution_metadata.count,
                    (unsigned long long) metadata_owned_count,
                    (unsigned long long) ctx->type_resolution_metadata_hits,
                    (unsigned long long) ctx->type_resolution_metadata_misses,
                    (unsigned long long) ctx->type_resolution_metadata_materializer_fallbacks);
            fprintf(stderr, "[type-res-stats] metadata-fallback: named=%llu generic_named=%llu compound=%llu other=%llu\n",
                    (unsigned long long) ctx->type_resolution_metadata_fallback_named,
                    (unsigned long long) ctx->type_resolution_metadata_fallback_generic_named,
                    (unsigned long long) ctx->type_resolution_metadata_fallback_compound,
                    (unsigned long long) ctx->type_resolution_metadata_fallback_other);
            fprintf(stderr, "[type-res-stats] metadata-fallback-named: builtin_shell=%llu generic_class=%llu alias=%llu non_class_symbol=%llu missing_symbol=%llu\n",
                    (unsigned long long) ctx->type_resolution_metadata_fallback_named_builtin_shell,
                    (unsigned long long) ctx->type_resolution_metadata_fallback_named_generic_class,
                    (unsigned long long) ctx->type_resolution_metadata_fallback_named_alias,
                    (unsigned long long) ctx->type_resolution_metadata_fallback_named_non_class_symbol,
                    (unsigned long long) ctx->type_resolution_metadata_fallback_named_missing_symbol);
            fprintf(stderr, "[type-res-stats] stage-legacy-family: generic_contract=%llu signature=%llu ability_consumer=%llu domain_contract=%llu alias=%llu other=%llu\n",
                    (unsigned long long) ctx->type_resolution_stage_legacy_generic_contract_count,
                    (unsigned long long) ctx->type_resolution_stage_legacy_signature_count,
                    (unsigned long long) ctx->type_resolution_stage_legacy_ability_consumer_count,
                    (unsigned long long) ctx->type_resolution_stage_legacy_domain_contract_count,
                    (unsigned long long) ctx->type_resolution_stage_legacy_alias_count,
                    (unsigned long long) ctx->type_resolution_stage_legacy_other_count);
            fprintf(stderr, "[type-res-stats] stage-alias: materialized=%llu diagnostic_fallback=%llu\n",
                    (unsigned long long) ctx->type_resolution_stage_alias_materialized_count,
                    (unsigned long long) ctx->type_resolution_stage_alias_diagnostic_fallback_count);
            fprintf(stderr, "[type-res-stats] stage-alias-fallback: resolver_calls=%llu resolved=%llu unresolved=%llu\n",
                    (unsigned long long) ctx->type_resolution_stage_alias_fallback_resolver_call_count,
                    (unsigned long long) ctx->type_resolution_stage_alias_fallback_resolved_count,
                    (unsigned long long) ctx->type_resolution_stage_alias_fallback_unresolved_count);
            {
                size_t total = g_resolve_type_node_cache_hits + g_resolve_type_node_cache_misses;
                fprintf(stderr, "[type-res-stats] cache: hits=%llu misses=%llu hit_rate=%.1f%%\n",
                        (unsigned long long) g_resolve_type_node_cache_hits,
                        (unsigned long long) g_resolve_type_node_cache_misses,
                        total > 0
                            ? 100.0 * (double)g_resolve_type_node_cache_hits / (double)total
                            : 0.0);
            }
            fprintf(stderr, "[type-res-stats] kind: TYPE_REF=%llu BUILTIN=%llu DECL=%llu ALIAS=%llu GENERIC_PARAM=%llu LOCAL_CONTRACT=%llu PROJECTION_PATH=%llu\n",
                    (unsigned long long) kind_counts[0], (unsigned long long) kind_counts[1], (unsigned long long) kind_counts[2],
                    (unsigned long long) kind_counts[3], (unsigned long long) kind_counts[4], (unsigned long long) kind_counts[5],
                    (unsigned long long) kind_counts[6]);

            /* Top 5 in-degree nodes */
            if (indeg != NULL && g->node_count > 0) {
                for (int rank = 0; rank < 5; rank++) {
                    size_t best = 0;
                    size_t best_val = 0;
                    bool found = false;
                    for (size_t i = 0; i < g->node_count; i++) {
                        if (indeg[i] > best_val) {
                            best = i; best_val = indeg[i]; found = true;
                        }
                    }
                    if (!found || best_val == 0) break;
                    fprintf(stderr, "[type-res-stats] top-indeg[%d] %s (in=%llu)\n",
                            rank,
                            g->nodes[best].label != NULL ? g->nodes[best].label : "<?>",
                            (unsigned long long) best_val);
                    indeg[best] = 0;
                }
            }
            free(indeg);
            free(outdeg);
            free(topo);
        }
    }

    free(topo_order);
    return !ctx->has_error;
}
