/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Generic call-site type-argument binding: inference, conflict and
 * missing-binding refusal, the sealed per-call binding MIR specializes from,
 * and where-clause validation against it.
 */

#include "type_checker_generic_diag_internal.h"
#include "type_checker_internal.h"
#include "diag_codes.h"

#include <stdlib.h>
#include <string.h>

/* A generic body copies a T value freely (`return x`, `[x, x]`), so a
 * single-owner runtime handle may not instantiate a type parameter: the body
 * could hand back a second binding to the caller's runtime storage. */
static void
generic_call_reject_single_owner_handle_arguments(ASTNode *expr,
                                                  SemanticContext *ctx,
                                                  ASTNode *decl,
                                                  GenericParams *decl_gp,
                                                  const char *display_name,
                                                  size_t provided,
                                                  Type **call_arg_types)
{
    for (size_t ai = 0; call_arg_types != NULL && ai < provided; ai++) {
        FuncParam *fp = ai < ast_func_param_count(decl)
            ? ast_func_param(decl, ai) : NULL;
        const char *param_type_name = fp != NULL && fp->type != NULL
            ? ast_type_name(fp->type) : NULL;
        if (param_type_name == NULL
            || find_generic_param_index(decl_gp, param_type_name) < 0
            || !type_is_single_owner_runtime_handle(call_arg_types[ai]))
            continue;
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
            PGY_CAUSE_MOVABLE_HANDLE_COPY_ATTEMPT,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            ai < ast_call_arg_count(expr) ? ast_call_argument(expr, ai) : expr,
            "'%s' is a single-owner runtime handle and cannot instantiate generic parameter '%s' of '%s'.\n"
            "Reason:\n"
            "- a generic body may return or store its parameter, which would give the caller's runtime storage a second binding\n"
            "Fix:\n"
            "- pass the value the handle holds instead\n"
            "- or declare the parameter with the concrete handle type",
            type_name_or_unknown(call_arg_types[ai]), param_type_name,
            display_name);
    }
}

/* Does the type reference `type_ref` mention generic parameter `name` (`T`,
 * `Array<T>`, `(T, Int)`)? A reference this walk cannot read, such as a
 * callable type, counts as mentioning it, so the unbound check below never
 * refuses a call it cannot judge. */
static bool
generic_type_ref_mentions(const ASTNode *type_ref, const char *name)
{
    GenericParams *args;

    if (type_ref == NULL || name == NULL)
        return false;
    if (type_ref->type != AST_TYPE)
        return true;
    if (ast_type_name(type_ref) != NULL
        && strcmp(ast_type_name(type_ref), name) == 0)
        return true;
    args = ast_type_generic_args(type_ref);
    for (size_t i = 0; i < ast_generic_param_count(args); i++) {
        GenericParam *arg = ast_generic_param_at(args, i);
        if (arg != NULL && generic_type_ref_mentions(arg->constraint, name))
            return true;
    }
    for (size_t i = 0; i < ast_type_tuple_element_count(type_ref); i++) {
        if (generic_type_ref_mentions(ast_type_tuple_element(type_ref, i),
                                      name))
            return true;
    }
    return false;
}

/* A generic parameter that no parameter type mentions and that has no
 * default cannot be bound from the call: `Make<T>() -> Option<T>` called as
 * `Make()` left T unbound, and MIR lowering refused the call after semantic
 * had admitted it. */
static bool
generic_call_reject_unbound_parameters(ASTNode *expr, SemanticContext *ctx,
                                       ASTNode *stmt, GenericParams *decl_gp,
                                       const char *display_name)
{
    if (ast_call_generic_args(expr) != NULL)
        return false; /* explicit type arguments bind every parameter */
    for (size_t gi = 0; gi < ast_generic_param_count(decl_gp); gi++) {
        GenericParam *gp = ast_generic_param_at(decl_gp, gi);
        const char *name = ast_generic_param_name(gp);
        bool mentioned = false;

        if (name == NULL || ast_generic_param_default_type(gp) != NULL)
            continue;
        for (size_t pi = 0; pi < ast_func_param_count(stmt) && !mentioned;
             pi++) {
            FuncParam *fp = ast_func_param(stmt, pi);
            mentioned = fp != NULL && generic_type_ref_mentions(fp->type, name);
        }
        if (mentioned)
            continue;
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_CALL_ARG_TYPE_MISMATCH,
            PGY_FIX_ALIGN_ARG_TYPE,
            expr,
            "Call to '%s' cannot bind generic parameter '%s': no parameter of '%s' mentions it.\n"
            "Reason:\n"
            "- a generic parameter is bound from the call's arguments\n"
            "- '%s' appears only in the result or body, so this call leaves it unbound\n"
            "Fix:\n"
            "- pass the type explicitly: %s<...>(...)\n"
            "- or add a parameter of type '%s', or a default type for it",
            display_name, name, display_name, name, display_name, name);
        return true;
    }
    return false;
}

/* ---------------------------------------------------------------------
 * Call-site type-argument binding (registry row mir.generic_specialization)
 *
 * The checker owns generic inference. Each generic parameter of the callee
 * takes one checked type per call: its explicit type argument, else the
 * matching component of the first argument whose parameter type mentions
 * it, else its declared default. The binding is sealed on the call node in
 * MIR's type grammar and MIR's specialization copies it; no later stage
 * re-derives a binding by matching type text.
 * ------------------------------------------------------------------- */

typedef struct GenericCallBindingRow {
    Type  *type;
    size_t source_argument; /* 1-based argument that bound it; 0 = explicit */
} GenericCallBindingRow;

typedef struct GenericCallBindingTable {
    SemanticContext       *ctx;
    ASTNode               *call;
    const char            *display_name;
    GenericParams         *params;
    GenericCallBindingRow *rows;
    size_t                 count;
} GenericCallBindingTable;

/* A type fixes a parameter only when no component is unresolved: None, []
 * and a contextual Ok/Err carry no type for it. */
static bool
generic_call_type_is_resolved(const Type *type)
{
    if (type == NULL || type == TYPE_UNKNOWN)
        return false;
    switch (type->kind) {
    case TYPE_KIND_CONSTRUCTED:
        for (size_t i = 0; i < type_constructed_arg_count(type); i++) {
            if (!generic_call_type_is_resolved(type_constructed_arg(type, i)))
                return false;
        }
        return true;
    case TYPE_KIND_TUPLE:
        for (size_t i = 0; i < type_tuple_arity(type); i++) {
            if (!generic_call_type_is_resolved(
                    type_tuple_get_element(type, i)))
                return false;
        }
        return true;
    case TYPE_KIND_SLOT:
        return generic_call_type_is_resolved(type_slot_inner_type(type));
    default:
        return true;
    }
}

static bool
generic_type_name_append(char **text, size_t *len, size_t *cap,
                         const char *piece, size_t piece_len)
{
    char *grown;
    size_t next;

    if (*len + piece_len + 1 > *cap) {
        next = *cap > 0 ? *cap : 32;
        while (next < *len + piece_len + 1)
            next *= 2;
        grown = realloc(*text, next);
        if (grown == NULL)
            return false;
        *text = grown;
        *cap = next;
    }
    memcpy(*text + *len, piece, piece_len);
    *len += piece_len;
    (*text)[*len] = '\0';
    return true;
}

/* MIR renders a declared type with its arguments joined by ',' and no
 * space, and the checker's own names put ", " between them, so a bound type
 * is rendered from its structure. A callable, Void, Never or unresolved type
 * has no spelling in MIR's grammar. */
static bool
generic_type_name_render(const Type *type, char **text, size_t *len,
                         size_t *cap)
{
    const char *head = "";
    size_t head_len = 0;
    size_t count = 1;
    const char *open = "<";
    const char *close = ">";

    if (!generic_call_type_is_resolved(type) || type == TYPE_VOID
        || type == TYPE_NEVER || type->name == NULL
        || type->kind == TYPE_KIND_FUNCTION)
        return false;
    if (type->kind == TYPE_KIND_CONSTRUCTED) {
        head = type_constructed_constructor(type) != NULL
            ? type_constructed_constructor(type)->name : NULL;
        if (head == NULL)
            return false;
        head_len = strlen(head);
        count = type_constructed_arg_count(type);
    } else if (type->kind == TYPE_KIND_TUPLE) {
        count = type_tuple_arity(type);
        open = "(";
        close = ")";
    } else if (type->kind == TYPE_KIND_SLOT) {
        head = type->name; /* "Slot<", "ReadView<", ... */
        head_len = strcspn(type->name, "<");
    } else {
        return generic_type_name_append(text, len, cap, type->name,
            strlen(type->name));
    }
    if (!generic_type_name_append(text, len, cap, head, head_len)
        || !generic_type_name_append(text, len, cap, open, 1))
        return false;
    for (size_t i = 0; i < count; i++) {
        const Type *component = type->kind == TYPE_KIND_CONSTRUCTED
            ? type_constructed_arg(type, i)
            : type->kind == TYPE_KIND_TUPLE
                ? type_tuple_get_element(type, i)
                : type_slot_inner_type(type);
        if ((i > 0 && !generic_type_name_append(text, len, cap, ",", 1))
            || !generic_type_name_render(component, text, len, cap))
            return false;
    }
    return generic_type_name_append(text, len, cap, close, 1);
}

bool
semantic_generic_call_seal_type_arguments(SemanticContext *ctx,
                                          ASTNode *call,
                                          const char *display_name,
                                          GenericParams *params,
                                          Type *const *types,
                                          size_t count)
{
    char **names;
    const char *callee = display_name != NULL ? display_name : "<call>";

    if (call == NULL || call->type != AST_CALL || types == NULL || count == 0)
        return false;
    names = calloc(count, sizeof(char *));
    if (names == NULL) {
        semantic_error(ctx, call,
            "Generic call type-argument binding allocation failed");
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        size_t len = 0;
        size_t cap = 0;
        const char *param_name;

        if (generic_type_name_render(types[i], &names[i], &len, &cap))
            continue;
        param_name = ast_generic_param_name(ast_generic_param_at(params, i));
        if (param_name == NULL)
            param_name = "<type-param>";
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
            PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
            call,
            "Call to '%s' binds generic parameter '%s' to '%s', which cannot instantiate it.\n"
            "Reason:\n"
            "- a generic callee is specialized once per type argument\n"
            "- a callable, Void, Never or unresolved type has no specialization\n"
            "Fix:\n"
            "- pass a value of a named type for '%s'\n"
            "- or declare the parameter with a concrete type",
            callee, param_name, type_name_or_unknown(types[i]), param_name);
        for (size_t j = 0; j < count; j++)
            free(names[j]);
        free(names);
        return false;
    }
    if (!ast_call_seal_semantic_generic_arg_type_names(call, names, count)) {
        for (size_t j = 0; j < count; j++)
            free(names[j]);
        free(names);
        semantic_error(ctx, call,
            "Generic call type-argument binding could not be recorded");
        return false;
    }
    return true;
}

/* One generic parameter takes one type per call. The first argument that
 * binds `T` fixes it; a later one must be assignable to that binding, and an
 * explicit type argument fixes it before any argument. Without this,
 * `Pick(1, "x")` and `Both(Some(1), "x")` were admitted and MIR lowering
 * refused the call, and `Keep<Int>("seven")` reached the C compiler. */
static bool
generic_call_bind_parameter(GenericCallBindingTable *table, size_t index,
                            Type *actual, size_t argument_index)
{
    GenericCallBindingRow *row = &table->rows[index];
    const char *name = ast_generic_param_name(
        ast_generic_param_at(table->params, index));

    if (name == NULL)
        name = "<type-param>";
    if (!generic_call_type_is_resolved(actual))
        return true;
    if (row->type == NULL) {
        row->type = actual;
        row->source_argument = argument_index + 1;
        return true;
    }
    if (type_is_assignable(actual, row->type))
        return true;
    if (row->source_argument == 0) {
        semantic_error_with_hints(table->ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_CALL_ARG_TYPE_MISMATCH, PGY_FIX_ALIGN_ARG_TYPE,
            table->call,
            "Call to '%s' binds generic parameter '%s' to '%s' by its explicit type argument, but argument %llu gives it '%s'.\n"
            "Reason:\n"
            "- an explicit type argument binds the parameter for the whole call\n"
            "- argument %llu is not assignable to it\n"
            "Fix:\n"
            "- pass an argument of type '%s'\n"
            "- or change the explicit type argument",
            table->display_name, name, type_name_or_unknown(row->type),
            (unsigned long long)(argument_index + 1),
            type_name_or_unknown(actual),
            (unsigned long long)(argument_index + 1),
            type_name_or_unknown(row->type));
        return false;
    }
    semantic_error_with_hints(table->ctx, PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_CALL_ARG_TYPE_MISMATCH, PGY_FIX_ALIGN_ARG_TYPE,
        table->call,
        "Call to '%s' binds generic parameter '%s' to both '%s' (argument %llu) and '%s' (argument %llu).\n"
        "Reason:\n"
        "- one generic parameter takes one type per call\n"
        "- argument %llu is not assignable to the type argument %llu bound\n"
        "Fix:\n"
        "- pass arguments of one type for '%s'\n"
        "- or give the parameters separate generic parameters",
        table->display_name, name, type_name_or_unknown(row->type),
        (unsigned long long)row->source_argument,
        type_name_or_unknown(actual),
        (unsigned long long)(argument_index + 1),
        (unsigned long long)(argument_index + 1),
        (unsigned long long)row->source_argument, name);
    return false;
}

static bool generic_call_bind_formal(GenericCallBindingTable *table,
                                     ASTNode *formal, Type *actual,
                                     size_t argument_index);

/* A constructed formal (Option<T>, Array<T>, Result<T, E>, a generic class
 * Box<T>, Slot<T>) binds from the same constructor's arguments in the
 * argument's checked type. Another shape binds nothing here; argument
 * assignability owns that diagnostic. */
static bool
generic_call_bind_components(GenericCallBindingTable *table, ASTNode *formal,
                             Type *actual, size_t argument_index)
{
    const char *name = ast_type_name(formal);
    GenericParams *formal_args = ast_type_generic_args(formal);
    size_t formal_count = ast_generic_param_count(formal_args);
    Type *constructor = type_constructed_constructor(actual);

    if (actual->kind == TYPE_KIND_SLOT) {
        size_t head_len = strcspn(actual->name, "<");
        if (formal_count != 1 || strlen(name) != head_len
            || strncmp(actual->name, name, head_len) != 0)
            return true;
        return generic_call_bind_formal(table,
            ast_generic_param_constraint(ast_generic_param_at(formal_args, 0)),
            type_slot_inner_type(actual), argument_index);
    }
    if (constructor == NULL || constructor->name == NULL
        || strcmp(constructor->name, name) != 0
        || type_constructed_arg_count(actual) != formal_count)
        return true;
    for (size_t i = 0; i < formal_count; i++) {
        if (!generic_call_bind_formal(table,
                ast_generic_param_constraint(
                    ast_generic_param_at(formal_args, i)),
                type_constructed_arg(actual, i), argument_index))
            return false;
    }
    return true;
}

static bool
generic_call_bind_formal(GenericCallBindingTable *table, ASTNode *formal,
                         Type *actual, size_t argument_index)
{
    int index;

    if (formal == NULL || actual == NULL || actual == TYPE_UNKNOWN
        || actual->name == NULL)
        return true;
    if (formal->type == AST_CHANNEL_TYPE || formal->type == AST_FUTURE_TYPE) {
        bool channel = formal->type == AST_CHANNEL_TYPE;
        if (!type_constructed_is(actual, channel ? TYPE_CHANNEL : TYPE_FUTURE,
                1))
            return true;
        return generic_call_bind_formal(table,
            channel ? ast_channel_type_element_type(formal)
                    : ast_future_type_value_type(formal),
            type_constructed_arg(actual, 0), argument_index);
    }
    if (formal->type != AST_TYPE)
        return true; /* a callable parameter type is not read here */
    if (ast_type_tuple_element_count(formal) > 0) {
        size_t count = ast_type_tuple_element_count(formal);
        if (!type_is_tuple(actual) || type_tuple_arity(actual) != count)
            return true;
        for (size_t i = 0; i < count; i++) {
            if (!generic_call_bind_formal(table,
                    ast_type_tuple_element(formal, i),
                    type_tuple_get_element(actual, i), argument_index))
                return false;
        }
        return true;
    }
    if (ast_type_name(formal) == NULL)
        return true;
    if (ast_generic_param_count(ast_type_generic_args(formal)) > 0)
        return generic_call_bind_components(table, formal, actual,
            argument_index);
    index = find_generic_param_index(table->params, ast_type_name(formal));
    if (index < 0 || (size_t)index >= table->count)
        return true;
    return generic_call_bind_parameter(table, (size_t)index, actual,
        argument_index);
}

static bool
generic_call_bind_explicit(GenericCallBindingTable *table)
{
    size_t provided = ast_call_generic_arg_count(table->call);

    if (provided > table->count) {
        semantic_error_with_hints(table->ctx, PGY_CODE_SEM_INFER_GENERIC,
            PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
            table->call,
            "Call to '%s' supplies %llu type argument(s), but '%s' declares %llu generic parameter(s).\n"
            "Reason:\n"
            "- each type argument binds one declared generic parameter\n"
            "Fix:\n"
            "- remove the extra type argument(s)",
            table->display_name, (unsigned long long)provided,
            table->display_name, (unsigned long long)table->count);
        return false;
    }
    for (size_t i = 0; i < provided; i++) {
        ASTNode *type_ref = ast_generic_param_constraint(
            ast_call_generic_arg(table->call, i));
        Type *resolved = type_ref != NULL
            ? semantic_type_resolution_lookup_metadata_type_ref(
                table->ctx, type_ref)
            : NULL;
        if (!generic_call_type_is_resolved(resolved)
            || type_equals(resolved, TYPE_VOID)) {
            semantic_error_with_hints(table->ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID,
                PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                type_ref != NULL ? type_ref : table->call,
                "Call to '%s' has type argument %llu that does not resolve to a value type.\n"
                "Reason:\n"
                "- an explicit type argument must name a declared value type\n"
                "Fix:\n"
                "- name a declared type, or omit the type argument and let the arguments bind it",
                table->display_name, (unsigned long long)(i + 1));
            return false;
        }
        table->rows[i].type = resolved;
    }
    return true;
}

static bool
generic_call_bind_arguments(GenericCallBindingTable *table, ASTNode *stmt,
                            size_t provided, Type **call_arg_types)
{
    for (size_t ai = 0; call_arg_types != NULL && ai < provided; ai++) {
        FuncParam *fp = ai < ast_func_param_count(stmt)
            ? ast_func_param(stmt, ai) : NULL;
        if (fp != NULL
            && !generic_call_bind_formal(table, fp->type, call_arg_types[ai],
                ai))
            return false;
    }
    return true;
}

/* A parameter no explicit type argument or argument bound takes its
 * default; without one the call is refused here, not in MIR lowering. */
static bool
generic_call_bind_defaults(GenericCallBindingTable *table)
{
    for (size_t i = 0; i < table->count; i++) {
        GenericParam *param = ast_generic_param_at(table->params, i);
        ASTNode *default_type = ast_generic_param_default_type(param);
        const char *name = ast_generic_param_name(param);

        if (table->rows[i].type != NULL)
            continue;
        if (default_type != NULL) {
            Type *resolved = semantic_type_resolution_lookup_metadata_type_ref(
                table->ctx, default_type);
            if (generic_call_type_is_resolved(resolved)) {
                table->rows[i].type = resolved;
                continue;
            }
        }
        if (name == NULL)
            name = "<type-param>";
        semantic_error_with_hints(table->ctx, PGY_CODE_SEM_INFER_GENERIC,
            PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
            table->call,
            "Call to '%s' cannot infer generic parameter '%s' from its arguments.\n"
            "Reason:\n"
            "- no argument has a resolved checked type for '%s' (None, [] and a contextual Ok/Err have none)\n"
            "- '%s' has no usable default type argument\n"
            "Fix:\n"
            "- pass the type explicitly: %s<...>(...)\n"
            "- or bind the argument to a local with a declared type first",
            table->display_name, name, name, name, table->display_name);
        return false;
    }
    return true;
}

/* The where-clause is checked against the sealed binding, so a bound on T
 * in `Peek<T>(o: Option<T>) where T: Sortable` sees T, not only a bare-T
 * argument. */
static void
generic_call_validate_where(ASTNode *expr, SemanticContext *ctx,
                            const char *display_name, GenericParams *decl_gp,
                            WhereClause *wc, Type **effective_generic_types)
{
    size_t decl_count = ast_generic_param_count(decl_gp);
    char *expected_sig = format_generic_subject_signature(display_name, decl_gp);

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        const char *param_name;
        Type *concrete_type;
        const char *actual_sig =
            format_effective_generic_type_list_scratch(
                ctx, display_name, effective_generic_types, decl_count);

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_name = tc->type_param;
        param_index = find_generic_param_index(decl_gp, param_name);
        if (param_index < 0 || (size_t)param_index >= decl_count) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_CLASS_CONTRACT_INVALID,
                PGY_CAUSE_CLASS_CONTRACT,
                PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN,
                expr,
                "Call site of '%s' could not validate generic parameter '%s'.\n"
                "Reason:\n"
                "- function '%s' where-clause references '%s'\n"
                "- that parameter does not exist in the function generic parameter list\n"
                "Fix:\n"
                "- change the function where-clause to reference an existing generic parameter\n"
                "- or add generic parameter '%s' to function '%s'",
                display_name,
                param_name != NULL ? param_name : "<param>",
                display_name,
                param_name != NULL ? param_name : "<param>",
                param_name != NULL ? param_name : "<param>",
                display_name);
            continue;
        }

        concrete_type = effective_generic_types[param_index];
        if (concrete_type == NULL) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_CLASS_CONTRACT_INVALID,
                PGY_CAUSE_CLASS_CONTRACT,
                PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN,
                expr,
                "Call site of '%s' could not materialize generic argument for '%s'.\n"
                "Reason:\n"
                "- function '%s' where-clause validation depends on an effective type argument for '%s'\n"
                "- neither explicit call-site evidence nor declaration defaults produced a concrete type\n"
                "Fix:\n"
                "- pass arguments that make '%s' concrete\n"
                "- or add/fix a default type argument for '%s'",
                display_name,
                param_name != NULL ? param_name : "<param>",
                display_name,
                param_name != NULL ? param_name : "<param>",
                param_name != NULL ? param_name : "<param>",
                param_name != NULL ? param_name : "<param>");
            continue;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                ast_type_name(tc->bounds[bi]) != NULL
                    ? ast_type_name(tc->bounds[bi])
                    : NULL;
            bool satisfies = concrete_type_satisfies_bound(
                concrete_type, tc->bounds[bi], ctx);
            if (!satisfies) {
                semantic_report_function_generic_bound_failure(
                    ctx,
                    expr,
                    display_name,
                    param_name,
                    bound_name != NULL ? bound_name : "<constraint>",
                    bounds_text,
                    expected_sig != NULL ? expected_sig : display_name,
                    actual_sig != NULL ? actual_sig : display_name,
                    concrete_type->name != NULL ? concrete_type->name : "<type>");
            }
            free(bounds_text);
        }
    }

    free(expected_sig);
}

void
semantic_validate_function_call_generic_where(ASTNode *expr,
                                              SemanticContext *ctx,
                                              const char *display_name,
                                              size_t provided,
                                              Type **call_arg_types)
{
    GenericCallBindingTable table;
    Type **bound_types;
    ASTNode *stmt;
    bool bound;

    /* A lexical callable value is not the same-named declaration. */
    if (ctx == NULL || display_name == NULL || expr == NULL
        || expr->type != AST_CALL
        || ast_call_semantic_callee_value_binding_id(expr) != 0)
        return;
    stmt = semantic_find_function_decl_by_name(ctx, display_name);
    if (stmt == NULL || ast_generic_param_count(ast_func_generic_params(stmt)) == 0)
        return;

    memset(&table, 0, sizeof(table));
    table.ctx = ctx;
    table.call = expr;
    table.display_name = display_name;
    table.params = ast_func_generic_params(stmt);
    table.count = ast_generic_param_count(table.params);
    generic_call_reject_single_owner_handle_arguments(
        expr, ctx, stmt, table.params, display_name, provided, call_arg_types);
    if (generic_call_reject_unbound_parameters(
            expr, ctx, stmt, table.params, display_name))
        return;
    table.rows = calloc(table.count, sizeof(*table.rows));
    bound_types = calloc(table.count, sizeof(*bound_types));
    if (table.rows == NULL || bound_types == NULL) {
        free(table.rows);
        free(bound_types);
        semantic_error(ctx, expr,
            "Generic binding table allocation failed while checking a call");
        return;
    }
    bound = generic_call_bind_explicit(&table)
        && generic_call_bind_arguments(&table, stmt, provided, call_arg_types)
        && generic_call_bind_defaults(&table);
    for (size_t i = 0; bound && i < table.count; i++)
        bound_types[i] = table.rows[i].type;
    if (bound
        && semantic_generic_call_seal_type_arguments(ctx, expr, display_name,
            table.params, bound_types, table.count)
        && ast_func_where_clause(stmt) != NULL)
        generic_call_validate_where(expr, ctx, display_name, table.params,
            ast_func_where_clause(stmt), bound_types);
    free(table.rows);
    free(bound_types);
}
