#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "type_checker_internal.h"
#include "diag_codes.h"

static Type *
program_lookup_dag_type_ref_or_unknown(ASTNode *type_node,
                                       SemanticContext *ctx)
{
    Type *resolved;

    /* Top-level placeholders must consume precollected DAG metadata only. */
    resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx,
                                                                type_node);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static Type *
program_lookup_dag_func_return_type_or_void(ASTNode *func_decl,
                                            SemanticContext *ctx)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL
        || ast_func_return_type(func_decl) == NULL) {
        return TYPE_VOID;
    }
    return program_lookup_dag_type_ref_or_unknown(
        ast_func_return_type(func_decl), ctx);
}

static Type *
program_lookup_dag_intent_binding_type_or_unknown(ASTNode *binding,
                                                  SemanticContext *ctx)
{
    if (binding == NULL)
        return TYPE_UNKNOWN;
    if (binding->type == AST_INTENT_INVOLVES)
        return program_lookup_dag_type_ref_or_unknown(
            ast_intent_involves_subject_type(binding), ctx);
    if (binding->type == AST_INTENT_VALUE)
        return program_lookup_dag_type_ref_or_unknown(
            ast_intent_value_type(binding), ctx);
    return TYPE_UNKNOWN;
}

static bool
program_report_resolution_oom(SemanticContext *ctx,
                              ASTNode *site,
                              const char *what)
{
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_UNKNOWN_TYPE,
        PGY_CAUSE_RESOLUTION_OOM,
        PGY_FIX_REDUCE_SCOPE_OR_RETRY,
        site,
        "Type-resolution prepass could not allocate metadata for %s.\n"
        "Reason:\n"
        "- graph-backed declaration prepass ran out of memory while building stable placeholder types\n"
        "Fix:\n"
        "- reduce this compilation unit size and retry\n"
        "- or report the input if this happens on a small program",
        what != NULL ? what : "a declaration");
    return false;
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
    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        if (stmt->type == AST_TYPE_ALIAS) {
            const char *tname = ast_type_alias_name(stmt);
            if (tname != NULL && scope_lookup_current(ctx->scope, tname) == NULL) {
                Symbol *s = symbol_create_function(tname, TYPE_UNKNOWN,
                    stmt->line, stmt->column);
                if (s == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "type-alias placeholder symbol");
                if (s != NULL)
                    s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_CLASS_DECL) {
            const char *cname = ast_class_name(stmt);
            if (cname != NULL && scope_lookup_current(ctx->scope, cname) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "class placeholder type");
                t->kind = TYPE_KIND_CLASS;
                t->nominal_flavor = nominal_flavor_from_decl(stmt);
                t->name = pergyra_strdup(cname);
                Symbol *s = symbol_create_function(cname,
                    t, stmt->line, stmt->column);
                if (s == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "class placeholder symbol");
                if (s != NULL)
                    s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_ABILITY_DECL) {
            const char *aname = ast_ability_name(stmt);
            if (aname != NULL && scope_lookup_current(ctx->scope, aname) == NULL) {
                Symbol *s = symbol_create_function(aname, TYPE_VOID,
                                                    stmt->line, stmt->column);
                if (s == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "ability placeholder symbol");
                if (s != NULL)
                    s->kind = SYMBOL_ABILITY;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_FUNC_DECL) {
            const char *fname = ast_declaration_name(stmt);
            if (fname == NULL)
                continue;
            if (scope_lookup_current(ctx->scope, fname) == NULL) {
                /* Forward-declare with correct param count so that
                 * call-site arity checks pass before Pass 2. */
                size_t fpc = ast_func_param_count(stmt);
                /* Exclude implicit 'self' param from count */
                size_t real_pc = 0;
                for (size_t j = 0; j < fpc; j++) {
                    FuncParam *p = ast_func_param(stmt, j);
                    if (p == NULL)
                        continue;
                    if (p->type == NULL && p->name != NULL
                        && strcmp(p->name, "self") == 0)
                        continue;
                    real_pc++;
                }
                Type **ptypes = calloc(real_pc > 0 ? real_pc : 1,
                                         sizeof(Type *));
                if (ptypes == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "function parameters");
                for (size_t j = 0; j < real_pc; j++)
                    ptypes[j] = TYPE_UNKNOWN;
                Type *ret = program_lookup_dag_func_return_type_or_void(stmt, ctx);
                Type *placeholder = type_create_function(ptypes, real_pc, ret);
                if (placeholder == NULL) {
                    free(ptypes);
                    return program_report_resolution_oom(ctx, stmt,
                        "function placeholder type");
                }
                if (placeholder != NULL)
                    type_function_set_effects(placeholder,
                        declared_effects_from_function_node(stmt, ctx, NULL));
                free(ptypes);
                Symbol *s = symbol_create_function(fname, placeholder,
                                                    stmt->line, stmt->column);
                if (s == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "function placeholder symbol");
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_EVENT_DECL) {
            const char *ename = ast_event_name(stmt);
            if (scope_lookup_current(ctx->scope, ename) == NULL) {
                size_t epc = ast_event_param_count(stmt);
                Type **eptypes = calloc(epc > 0 ? epc : 1, sizeof(Type *));
                if (eptypes == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "event parameters");
                for (size_t j = 0; j < epc; j++) {
                    ASTNode *p = ast_event_param(stmt, j);
                    if (p != NULL
                        && p->type == AST_LET_DECL
                        && ast_let_type(p) != NULL) {
                        eptypes[j] = program_lookup_dag_type_ref_or_unknown(
                            ast_let_type(p), ctx);
                    } else {
                        eptypes[j] = TYPE_UNKNOWN;
                    }
                }
                Type *evt_ft = type_create_function(eptypes, epc, TYPE_VOID);
                if (evt_ft == NULL) {
                    free(eptypes);
                    return program_report_resolution_oom(ctx, stmt,
                        "event placeholder type");
                }
                free(eptypes);
                Symbol *s = symbol_create_function(ename, evt_ft,
                                                    stmt->line, stmt->column);
                if (s == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "event placeholder symbol");
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_ENUM_DECL) {
            const char *ename = ast_enum_name(stmt);
            size_t variant_count = 0;
            char **variants = ast_enum_variants(stmt, &variant_count);
            if (ename != NULL && scope_lookup_current(ctx->scope, ename) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "enum placeholder type");
                t->kind = TYPE_KIND_ENUM;
                t->name = pergyra_strdup(ename);
                Symbol *s = symbol_create_function(ename,
                    t, stmt->line, stmt->column);
                if (s == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "enum placeholder symbol");
                s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
            Symbol *enum_sym = scope_lookup_current(ctx->scope, ename);
            Type *etype = enum_sym != NULL ? enum_sym->type : TYPE_UNKNOWN;
            for (size_t j = 0; j < variant_count; j++) {
                const char *vname = variants != NULL ? variants[j] : NULL;
                if (vname == NULL || scope_lookup_current(ctx->scope, vname) != NULL)
                    continue;
                size_t vpc = ast_enum_variant_param_count(stmt, j);
                if (vpc > 0) {
                    /* Tagged union variant constructor: register as function
                     * Circle(Int) -> Shape */
                    Type **ptypes = calloc(vpc, sizeof(Type *));
                    if (ptypes == NULL)
                        return program_report_resolution_oom(ctx, stmt,
                            "enum variant parameters");
                    for (size_t p = 0; p < vpc && ptypes != NULL; p++) {
                        ASTNode *pt = ast_enum_variant_param(stmt, j, p);
                        ptypes[p] =
                            program_lookup_dag_type_ref_or_unknown(pt, ctx);
                    }
                    Type *ft = type_create_function(ptypes, vpc, etype);
                    if (ft == NULL) {
                        free(ptypes);
                        return program_report_resolution_oom(ctx, stmt,
                            "enum variant constructor type");
                    }
                    free(ptypes);
                    Symbol *vs = symbol_create_function(vname, ft,
                        stmt->line, stmt->column);
                    if (vs == NULL)
                        return program_report_resolution_oom(ctx, stmt,
                            "enum variant constructor symbol");
                    scope_declare(ctx->scope, vs);
                } else {
                    /* Simple variant: register as variable */
                    Symbol *vs = symbol_create_variable(vname, etype,
                        stmt->line, stmt->column);
                    if (vs == NULL)
                        return program_report_resolution_oom(ctx, stmt,
                            "enum variant symbol");
                    scope_declare(ctx->scope, vs);
                }
            }
        } else if (stmt->type == AST_EXTERN_BLOCK) {
            size_t extern_count = 0;
            (void)ast_extern_block_declarations(stmt, &extern_count);
            for (size_t j = 0; j < extern_count; j++) {
                ASTNode *decl = ast_extern_block_declaration(stmt, j);
                if (decl == NULL || decl->type != AST_FUNC_DECL)
                    continue;
                const char *fname = ast_declaration_name(decl);
                if (fname == NULL)
                    continue;
                if (scope_lookup_current(ctx->scope, fname) == NULL) {
                    Type *placeholder = type_create_function(NULL, 0, TYPE_VOID);
                    if (placeholder != NULL)
                        type_function_set_effects(placeholder,
                            declared_effects_from_function_node(decl, ctx, NULL));
                    if (placeholder == NULL)
                        return program_report_resolution_oom(ctx, decl,
                            "extern placeholder type");
                    Symbol *s = symbol_create_function(fname, placeholder,
                                                        decl->line, decl->column);
                    if (s == NULL)
                        return program_report_resolution_oom(ctx, decl,
                            "extern placeholder symbol");
                    scope_declare(ctx->scope, s);
                }
            }
        } else if (stmt->type == AST_INTENT_DECL) {
            const char *iname = ast_intent_decl_name(stmt);
            if (iname != NULL && scope_lookup_current(ctx->scope, iname) == NULL) {
                size_t binding_count = ast_intent_decl_binding_count(stmt);
                size_t involve_count = ast_intent_decl_involve_count(stmt);
                size_t value_count = ast_intent_decl_value_count(stmt);
                size_t ipc = binding_count > 0
                    ? binding_count
                    : (involve_count + value_count);
                ASTNode **bindings = ast_intent_decl_bindings(stmt, NULL);
                ASTNode **involves = ast_intent_decl_involves(stmt, NULL);
                ASTNode **values = ast_intent_decl_values(stmt, NULL);
                Type **ptypes = calloc(ipc > 0 ? ipc : 1, sizeof(Type *));
                if (ptypes == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "intent bindings");
                for (size_t j = 0; j < ipc; j++) {
                    ASTNode *binding = binding_count > 0
                        ? bindings[j]
                        : (j < involve_count
                            ? involves[j]
                            : values[j - involve_count]);
                    if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                        && ast_intent_involves_subject_type(binding) != NULL) {
                        ptypes[j] = program_lookup_dag_intent_binding_type_or_unknown(
                            binding, ctx);
                    } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                        && ast_intent_value_type(binding) != NULL) {
                        ptypes[j] = program_lookup_dag_intent_binding_type_or_unknown(
                            binding, ctx);
                    } else {
                        ptypes[j] = TYPE_UNKNOWN;
                    }
                }
                Type *ft = type_create_function(ptypes, ipc, TYPE_BOOL);
                if (ft == NULL) {
                    free(ptypes);
                    return program_report_resolution_oom(ctx, stmt,
                        "intent placeholder type");
                }
                free(ptypes);
                Symbol *s = symbol_create_function(iname,
                    ft, stmt->line, stmt->column);
                if (s == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "intent placeholder symbol");
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
                dname = ast_party_name(stmt);
            else if (stmt->type == AST_ROSTER_DECL)
                dname = ast_roster_name(stmt);
            else if (stmt->type == AST_WORLD_DECL)
                dname = ast_world_name(stmt);
            else if (stmt->type == AST_RELATION_DECL)
                dname = ast_relation_name(stmt);
            else if (stmt->type == AST_EFFECT_DECL)
                dname = ast_effect_name(stmt);
            else
                dname = ast_zone_name(stmt);
            if (dname != NULL && scope_lookup_current(ctx->scope, dname) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "domain placeholder type");
                t->kind = TYPE_KIND_CLASS;
                t->nominal_flavor = TYPE_NOMINAL_CLASS;
                t->name = pergyra_strdup(dname);
                Symbol *s = symbol_create_function(dname,
                    t, stmt->line, stmt->column);
                if (s == NULL)
                    return program_report_resolution_oom(ctx, stmt,
                        "domain placeholder symbol");
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
        free(topo_order);
        return program_report_resolution_oom(ctx, program,
            "type-resolution topological order");
    }

    semantic_run_type_resolution_worklist(program, ctx, topo_order, topo_count);

    /*
     * Pass 2: full type-check
     */
    for (size_t i = 0; i < ast_program_statement_count(program); i++)
        type_check_statement(ast_program_statement(program, i), ctx);

    (void)type_resolution_validate_graph(ctx);
    semantic_maybe_print_type_resolution_stats(ctx);

    free(topo_order);
    return !ctx->has_error;
}
