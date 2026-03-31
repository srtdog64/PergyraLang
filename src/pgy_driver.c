/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy — Pergyra compiler driver
 *
 * Pipeline:
 *   .pgy source
 *     → Lexer
 *     → Parser   → AST
 *     → Semantic → annotated AST  (errors abort here)
 *     → HIR      → lowered program buckets
 *     → LLVM backend (default when enabled) → object → native binary
 *       or C backend (fallback)             → .c     → native binary
 *
 * Usage:
 *   pgy <source.pgy>              compile → binary
 *   pgy <source.pgy> --emit-c     stop after generating C
 *   pgy <source.pgy> -o <out>     name the emitted native binary
 *   pgy <source.pgy> --emit-c -o <out.c>
 *   pgy <source.pgy> --run        compile + run
 *   pgy --tokens <source.pgy>     dump token stream
 *   pgy --ast    <source.pgy>     dump AST
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <process.h>   /* _getpid */
#define getpid _getpid
#else
#include <unistd.h>    /* getpid */
#endif

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/hir.h"
#include "compiler/compiler.h"
#include "common/string_compat.h"

typedef enum
{
    BACKEND_C,
    BACKEND_LLVM
} BackendKind;

typedef struct
{
    const char *source_path;
    const char *output_path;
    bool        emit_c_only;
    bool        emit_llvm_ir;
    bool        do_run;
    bool        dump_tokens;
    bool        dump_ast;
    bool        dump_hir;
    bool        verbose;
    bool        repl;
    BackendKind backend;
} DriverFlags;

typedef struct
{
    ASTNode **items;
    size_t    count;
    size_t    capacity;
} ASTVec;

typedef struct
{
    char *old_name;
    char *new_name;
} RenameEntry;

typedef struct RenameScope
{
    struct RenameScope *parent;
    RenameEntry        *entries;
    size_t              count;
    size_t              capacity;
} RenameScope;

typedef struct
{
    char  **names;
    size_t  count;
    size_t  capacity;
} ShadowNames;

static char *
read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "pgy: cannot open '%s'\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    char *buf = malloc((size_t)sz + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }

    size_t read_len = fread(buf, 1, (size_t)sz, f);
    if (read_len != (size_t)sz) {
        fclose(f);
        free(buf);
        fprintf(stderr, "pgy: failed to read '%s'\n", path);
        return NULL;
    }
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

static bool
astvec_push(ASTVec *vec, ASTNode *node)
{
    if (vec->count == vec->capacity) {
        size_t next = vec->capacity == 0 ? 8 : vec->capacity * 2;
        ASTNode **grown = realloc(vec->items, next * sizeof(ASTNode *));
        if (grown == NULL)
            return false;
        vec->items = grown;
        vec->capacity = next;
    }
    vec->items[vec->count++] = node;
    return true;
}

static char *
join_names(const char *a, const char *b)
{
    size_t alen = a != NULL ? strlen(a) : 0;
    size_t blen = b != NULL ? strlen(b) : 0;
    char *result = malloc(alen + blen + 1);
    if (result == NULL)
        return NULL;
    if (alen > 0)
        memcpy(result, a, alen);
    if (blen > 0)
        memcpy(result + alen, b, blen);
    result[alen + blen] = '\0';
    return result;
}

static char *
namespace_prefix_join(const char *prefix, const char *name)
{
    size_t plen = prefix != NULL ? strlen(prefix) : 0;
    size_t nlen = strlen(name);
    char *result = malloc(plen + nlen + 2);
    if (result == NULL)
        return NULL;
    if (plen > 0)
        memcpy(result, prefix, plen);
    memcpy(result + plen, name, nlen);
    result[plen + nlen] = '_';
    result[plen + nlen + 1] = '\0';
    return result;
}

static bool
rename_scope_add(RenameScope *scope, const char *old_name, const char *new_name)
{
    if (scope->count == scope->capacity) {
        size_t next = scope->capacity == 0 ? 8 : scope->capacity * 2;
        RenameEntry *grown = realloc(scope->entries, next * sizeof(RenameEntry));
        if (grown == NULL)
            return false;
        scope->entries = grown;
        scope->capacity = next;
    }
    scope->entries[scope->count].old_name = pergyra_strdup(old_name);
    scope->entries[scope->count].new_name = pergyra_strdup(new_name);
    if (scope->entries[scope->count].old_name == NULL
        || scope->entries[scope->count].new_name == NULL) {
        free(scope->entries[scope->count].old_name);
        free(scope->entries[scope->count].new_name);
        return false;
    }
    scope->count++;
    return true;
}

static const char *
rename_scope_lookup(const RenameScope *scope, const char *name)
{
    for (const RenameScope *s = scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->count; i++) {
            if (strcmp(s->entries[i].old_name, name) == 0)
                return s->entries[i].new_name;
        }
    }
    return NULL;
}

static void
rename_scope_destroy(RenameScope *scope)
{
    if (scope == NULL)
        return;
    for (size_t i = 0; i < scope->count; i++) {
        free(scope->entries[i].old_name);
        free(scope->entries[i].new_name);
    }
    free(scope->entries);
    scope->entries = NULL;
    scope->count = 0;
    scope->capacity = 0;
}

static bool
shadow_push(ShadowNames *shadow, const char *name)
{
    if (name == NULL)
        return true;
    if (shadow->count == shadow->capacity) {
        size_t next = shadow->capacity == 0 ? 8 : shadow->capacity * 2;
        char **grown = realloc(shadow->names, next * sizeof(char *));
        if (grown == NULL)
            return false;
        shadow->names = grown;
        shadow->capacity = next;
    }
    shadow->names[shadow->count++] = pergyra_strdup(name);
    return shadow->names[shadow->count - 1] != NULL;
}

static bool
shadow_contains(const ShadowNames *shadow, const char *name)
{
    for (size_t i = shadow->count; i > 0; i--) {
        if (strcmp(shadow->names[i - 1], name) == 0)
            return true;
    }
    return false;
}

static void
shadow_pop_to(ShadowNames *shadow, size_t saved_count)
{
    while (shadow->count > saved_count) {
        free(shadow->names[shadow->count - 1]);
        shadow->count--;
    }
}

static void
shadow_destroy(ShadowNames *shadow)
{
    shadow_pop_to(shadow, 0);
    free(shadow->names);
    shadow->names = NULL;
    shadow->capacity = 0;
}

static char **
node_name_slot(ASTNode *node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
        case AST_FUNC_DECL:
            return &node->data.func_decl.name;
        case AST_CLASS_DECL:
            return &node->data.class_decl.name;
        case AST_LET_DECL:
            return &node->data.let_decl.name;
        case AST_ACTOR_DECL:
            return &node->data.actor_decl.name;
        case AST_ABILITY_DECL:
            return &node->data.ability_decl.name;
        case AST_ROLE_DECL:
            return &node->data.role_decl.name;
        case AST_PARTY_DECL:
            return &node->data.party_decl.name;
        case AST_SYSTEMIC_DECL:
            return &node->data.systemic_decl.name;
        case AST_WORLD_DECL:
            return &node->data.world_decl.name;
        case AST_EVENT_DECL:
            return &node->data.event_decl.name;
        case AST_ENUM_DECL:
            return &node->data.enum_decl.name;
        default:
            return NULL;
    }
}

static bool
module_has_explicit_exports_in_stmt(ASTNode *node)
{
    if (node == NULL)
        return false;
    if (node->is_exported)
        return true;
    if (node->type == AST_NAMESPACE_DECL) {
        for (size_t i = 0; i < node->data.namespace_decl.count; i++) {
            if (module_has_explicit_exports_in_stmt(node->data.namespace_decl.statements[i]))
                return true;
        }
    }
    return false;
}

static bool
module_has_explicit_exports(ASTNode *program)
{
    if (program == NULL || program->type != AST_PROGRAM)
        return false;
    for (size_t i = 0; i < program->data.program.count; i++) {
        if (module_has_explicit_exports_in_stmt(program->data.program.statements[i]))
            return true;
    }
    return false;
}

static void normalize_node_refs(ASTNode *node, RenameScope *scope, ShadowNames *shadow);

static void
normalize_generic_params(GenericParams *params, RenameScope *scope, ShadowNames *shadow)
{
    if (params == NULL)
        return;
    size_t saved = shadow->count;
    for (size_t i = 0; i < params->count; i++) {
        if (params->params[i] != NULL && params->params[i]->name != NULL)
            shadow_push(shadow, params->params[i]->name);
    }
    for (size_t i = 0; i < params->count; i++) {
        GenericParam *param = params->params[i];
        if (param == NULL)
            continue;
        normalize_node_refs(param->constraint, scope, shadow);
        normalize_node_refs(param->default_type, scope, shadow);
    }
    shadow_pop_to(shadow, saved);
}

static void
normalize_type_node(ASTNode *node, RenameScope *scope, ShadowNames *shadow)
{
    if (node == NULL || node->type != AST_TYPE)
        return;

    if (node->data.type.name != NULL && !shadow_contains(shadow, node->data.type.name)) {
        const char *replacement = rename_scope_lookup(scope, node->data.type.name);
        if (replacement != NULL && strcmp(node->data.type.name, replacement) != 0) {
            free(node->data.type.name);
            node->data.type.name = pergyra_strdup(replacement);
        }
    }

    if (node->data.type.generic_args != NULL) {
        for (size_t i = 0; i < node->data.type.generic_args->count; i++) {
            GenericParam *arg = node->data.type.generic_args->params[i];
            if (arg == NULL)
                continue;
            normalize_node_refs(arg->constraint, scope, shadow);
            normalize_node_refs(arg->default_type, scope, shadow);
        }
    }
}

static void
normalize_call_args(ASTNode **args, size_t count, RenameScope *scope, ShadowNames *shadow)
{
    for (size_t i = 0; i < count; i++)
        normalize_node_refs(args[i], scope, shadow);
}

static void
normalize_node_refs(ASTNode *node, RenameScope *scope, ShadowNames *shadow)
{
    if (node == NULL)
        return;

    switch (node->type) {
        case AST_PROGRAM:
        case AST_NAMESPACE_DECL:
        case AST_IMPORT_DECL:
        case AST_ENUM_DECL:
            return;

        case AST_IDENTIFIER:
            if (node->data.identifier.name != NULL
                && !shadow_contains(shadow, node->data.identifier.name)) {
                const char *replacement =
                    rename_scope_lookup(scope, node->data.identifier.name);
                if (replacement != NULL
                    && strcmp(node->data.identifier.name, replacement) != 0) {
                    free(node->data.identifier.name);
                    node->data.identifier.name = pergyra_strdup(replacement);
                }
            }
            return;

        case AST_TYPE:
            normalize_type_node(node, scope, shadow);
            return;

        case AST_LET_DECL:
            normalize_node_refs(node->data.let_decl.type, scope, shadow);
            normalize_node_refs(node->data.let_decl.initializer, scope, shadow);
            return;

        case AST_FUNC_DECL: {
            normalize_generic_params(node->data.func_decl.generic_params, scope, shadow);
            size_t saved = shadow->count;
            if (node->data.func_decl.generic_params != NULL) {
                for (size_t i = 0; i < node->data.func_decl.generic_params->count; i++) {
                    GenericParam *gp = node->data.func_decl.generic_params->params[i];
                    if (gp != NULL && gp->name != NULL)
                        shadow_push(shadow, gp->name);
                }
            }
            for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
                FuncParam *param = node->data.func_decl.params[i];
                if (param == NULL)
                    continue;
                normalize_node_refs(param->type, scope, shadow);
                normalize_node_refs(param->default_value, scope, shadow);
                shadow_push(shadow, param->name);
            }
            normalize_node_refs(node->data.func_decl.return_type, scope, shadow);
            normalize_node_refs(node->data.func_decl.body, scope, shadow);
            shadow_pop_to(shadow, saved);
            return;
        }

        case AST_CLASS_DECL:
            normalize_generic_params(node->data.class_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
                if (node->data.class_decl.fields[i] != NULL)
                    normalize_node_refs(node->data.class_decl.fields[i]->type, scope, shadow);
            }
            for (size_t i = 0; i < node->data.class_decl.method_count; i++)
                normalize_node_refs(node->data.class_decl.methods[i], scope, shadow);
            return;

        case AST_ACTOR_DECL:
            normalize_generic_params(node->data.actor_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.actor_decl.field_count; i++) {
                if (node->data.actor_decl.fields[i] != NULL)
                    normalize_node_refs(node->data.actor_decl.fields[i]->type, scope, shadow);
            }
            for (size_t i = 0; i < node->data.actor_decl.method_count; i++)
                normalize_node_refs(node->data.actor_decl.methods[i], scope, shadow);
            return;

        case AST_ABILITY_DECL:
            for (size_t i = 0; i < node->data.ability_decl.require_count; i++)
                normalize_node_refs(node->data.ability_decl.require_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.ability_decl.method_count; i++)
                normalize_node_refs(node->data.ability_decl.methods[i], scope, shadow);
            return;

        case AST_ROLE_DECL:
            normalize_node_refs(node->data.role_decl.for_type, scope, shadow);
            normalize_generic_params(node->data.role_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.role_decl.include_count; i++)
                normalize_node_refs(node->data.role_decl.includes[i], scope, shadow);
            for (size_t i = 0; i < node->data.role_decl.impl_count; i++)
                normalize_node_refs(node->data.role_decl.impl_abilities[i], scope, shadow);
            normalize_node_refs(node->data.role_decl.parallel_block, scope, shadow);
            return;

        case AST_PARTY_DECL:
            normalize_node_refs(node->data.party_decl.extends, scope, shadow);
            normalize_generic_params(node->data.party_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.party_decl.role_count; i++)
                normalize_node_refs(node->data.party_decl.role_slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.party_decl.shared_count; i++)
                normalize_node_refs(node->data.party_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.party_decl.method_count; i++)
                normalize_node_refs(node->data.party_decl.methods[i], scope, shadow);
            return;

        case AST_SYSTEMIC_DECL:
            normalize_generic_params(node->data.systemic_decl.generic_params, scope, shadow);
            for (size_t i = 0; i < node->data.systemic_decl.party_count; i++)
                normalize_node_refs(node->data.systemic_decl.party_slots[i], scope, shadow);
            for (size_t i = 0; i < node->data.systemic_decl.shared_count; i++)
                normalize_node_refs(node->data.systemic_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.systemic_decl.method_count; i++)
                normalize_node_refs(node->data.systemic_decl.methods[i], scope, shadow);
            return;

        case AST_WORLD_DECL:
            for (size_t i = 0; i < node->data.world_decl.systemic_count; i++)
                normalize_node_refs(node->data.world_decl.systemics[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.shared_count; i++)
                normalize_node_refs(node->data.world_decl.shared_fields[i], scope, shadow);
            for (size_t i = 0; i < node->data.world_decl.method_count; i++)
                normalize_node_refs(node->data.world_decl.methods[i], scope, shadow);
            return;

        case AST_EVENT_DECL:
            for (size_t i = 0; i < node->data.event_decl.param_count; i++)
                normalize_node_refs(node->data.event_decl.params[i], scope, shadow);
            normalize_node_refs(node->data.event_decl.return_type, scope, shadow);
            return;

        case AST_REQUIRE_FIELD:
            normalize_node_refs(node->data.require_field.type, scope, shadow);
            return;

        case AST_PARTY_SHARED:
            normalize_node_refs(node->data.party_shared.type, scope, shadow);
            normalize_node_refs(node->data.party_shared.initializer, scope, shadow);
            return;

        case AST_SYSTEMIC_SLOT:
            if (node->data.systemic_slot.party_type != NULL) {
                const char *replacement =
                    rename_scope_lookup(scope, node->data.systemic_slot.party_type);
                if (replacement != NULL
                    && strcmp(node->data.systemic_slot.party_type, replacement) != 0) {
                    free(node->data.systemic_slot.party_type);
                    node->data.systemic_slot.party_type = pergyra_strdup(replacement);
                }
            }
            return;

        case AST_WORLD_SYSTEMIC:
            if (node->data.world_systemic.systemic_type != NULL) {
                const char *replacement =
                    rename_scope_lookup(scope, node->data.world_systemic.systemic_type);
                if (replacement != NULL
                    && strcmp(node->data.world_systemic.systemic_type, replacement) != 0) {
                    free(node->data.world_systemic.systemic_type);
                    node->data.world_systemic.systemic_type = pergyra_strdup(replacement);
                }
            }
            normalize_node_refs(node->data.world_systemic.initializer, scope, shadow);
            return;

        case AST_BLOCK:
        case AST_ASYNC_BLOCK: {
            size_t saved = shadow->count;
            ASTNode **stmts = node->type == AST_BLOCK
                ? node->data.block.statements
                : node->data.async_block.statements;
            size_t count = node->type == AST_BLOCK
                ? node->data.block.count
                : node->data.async_block.statement_count;
            for (size_t i = 0; i < count; i++) {
                ASTNode *stmt = stmts[i];
                normalize_node_refs(stmt, scope, shadow);
                if (stmt != NULL && stmt->type == AST_LET_DECL)
                    shadow_push(shadow, stmt->data.let_decl.name);
                if (stmt != NULL && stmt->type == AST_FOR_LOOP
                    && stmt->data.for_loop.variable != NULL)
                    shadow_push(shadow, stmt->data.for_loop.variable);
            }
            shadow_pop_to(shadow, saved);
            return;
        }

        case AST_WITH_STMT: {
            normalize_node_refs(node->data.with_stmt.slot_type, scope, shadow);
            size_t saved = shadow->count;
            if (node->data.with_stmt.alias != NULL)
                shadow_push(shadow, node->data.with_stmt.alias);
            normalize_node_refs(node->data.with_stmt.body, scope, shadow);
            shadow_pop_to(shadow, saved);
            return;
        }

        case AST_FOR_LOOP: {
            normalize_node_refs(node->data.for_loop.range_start, scope, shadow);
            normalize_node_refs(node->data.for_loop.range_end, scope, shadow);
            size_t saved = shadow->count;
            shadow_push(shadow, node->data.for_loop.variable);
            normalize_node_refs(node->data.for_loop.body, scope, shadow);
            shadow_pop_to(shadow, saved);
            return;
        }

        case AST_WHILE_LOOP:
            normalize_node_refs(node->data.while_loop.condition, scope, shadow);
            normalize_node_refs(node->data.while_loop.body, scope, shadow);
            return;

        case AST_IF_STMT:
            normalize_node_refs(node->data.if_stmt.condition, scope, shadow);
            normalize_node_refs(node->data.if_stmt.then_branch, scope, shadow);
            normalize_node_refs(node->data.if_stmt.else_branch, scope, shadow);
            return;

        case AST_RETURN:
            normalize_node_refs(node->data.return_stmt.value, scope, shadow);
            return;

        case AST_BINARY:
            normalize_node_refs(node->data.binary.left, scope, shadow);
            normalize_node_refs(node->data.binary.right, scope, shadow);
            return;

        case AST_UNARY:
            normalize_node_refs(node->data.unary.operand, scope, shadow);
            return;

        case AST_CALL:
            normalize_node_refs(node->data.call.callee, scope, shadow);
            normalize_call_args(node->data.call.arguments,
                                node->data.call.arg_count,
                                scope, shadow);
            return;

        case AST_MEMBER_ACCESS:
            normalize_node_refs(node->data.member.object, scope, shadow);
            return;

        case AST_ARRAY_ACCESS:
            normalize_node_refs(node->data.array_access.array, scope, shadow);
            normalize_node_refs(node->data.array_access.index, scope, shadow);
            return;

        case AST_ARRAY_LITERAL:
            normalize_call_args(node->data.array_literal.elements,
                                node->data.array_literal.count,
                                scope, shadow);
            return;

        case AST_ASSIGNMENT:
            normalize_node_refs(node->data.assignment.target, scope, shadow);
            normalize_node_refs(node->data.assignment.value, scope, shadow);
            return;

        case AST_AWAIT_EXPR:
            normalize_node_refs(node->data.await_expr.expression, scope, shadow);
            return;

        case AST_CHANNEL_SEND:
            normalize_node_refs(node->data.channel_send.channel, scope, shadow);
            normalize_node_refs(node->data.channel_send.value, scope, shadow);
            return;

        case AST_CHANNEL_RECV:
            normalize_node_refs(node->data.channel_recv.channel, scope, shadow);
            return;

        case AST_SELECT_STMT:
            normalize_call_args(node->data.select_stmt.cases,
                                node->data.select_stmt.case_count,
                                scope, shadow);
            normalize_node_refs(node->data.select_stmt.default_case, scope, shadow);
            return;

        case AST_MATCH_STMT:
            normalize_node_refs(node->data.match_stmt.subject, scope, shadow);
            normalize_call_args(node->data.match_stmt.cases,
                                node->data.match_stmt.case_count,
                                scope, shadow);
            normalize_node_refs(node->data.match_stmt.default_body, scope, shadow);
            return;

        case AST_MATCH_CASE:
            normalize_node_refs(node->data.match_case.pattern, scope, shadow);
            normalize_node_refs(node->data.match_case.guard, scope, shadow);
            normalize_node_refs(node->data.match_case.body, scope, shadow);
            return;

        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
            normalize_node_refs(node->data.event_op.event, scope, shadow);
            normalize_node_refs(node->data.event_op.handler, scope, shadow);
            return;

        case AST_EVENT_INVOKE:
            normalize_node_refs(node->data.event_invoke.event, scope, shadow);
            normalize_call_args(node->data.event_invoke.arguments,
                                node->data.event_invoke.arg_count,
                                scope, shadow);
            return;

        case AST_UNSAFE_BLOCK:
            normalize_node_refs(node->data.unsafe_block.body, scope, shadow);
            return;

        case AST_DEFER_STMT:
            normalize_node_refs(node->data.defer_stmt.body, scope, shadow);
            return;

        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:
        case AST_BREAK:
        case AST_CONTINUE:
            return;

        default:
            return;
    }
}

static void
free_namespace_shell(ASTNode *node)
{
    if (node == NULL || node->type != AST_NAMESPACE_DECL)
        return;
    free(node->data.namespace_decl.name);
    free(node->data.namespace_decl.statements);
    node->data.namespace_decl.name = NULL;
    node->data.namespace_decl.statements = NULL;
    node->data.namespace_decl.count = 0;
    free(node);
}

static bool
normalize_statement_list(ASTNode **statements, size_t count,
                         const char *public_prefix,
                         const char *private_prefix,
                         bool imported,
                         bool has_explicit_exports,
                         bool inherited_export,
                         RenameScope *parent_scope,
                         ASTVec *flat)
{
    RenameScope scope = { .parent = parent_scope };
    ShadowNames shadow = {0};

    for (size_t i = 0; i < count; i++) {
        ASTNode *stmt = statements[i];
        char **name_slot;
        if (stmt == NULL || stmt->type == AST_NAMESPACE_DECL)
            continue;
        name_slot = node_name_slot(stmt);
        if (name_slot == NULL || *name_slot == NULL)
            continue;

        char *public_name = join_names(public_prefix, *name_slot);
        bool visible = !imported || !has_explicit_exports
            || inherited_export || stmt->is_exported;
        char *final_name = visible
            ? pergyra_strdup(public_name)
            : join_names(private_prefix, public_name);
        if (public_name == NULL || final_name == NULL) {
            free(public_name);
            free(final_name);
            rename_scope_destroy(&scope);
            shadow_destroy(&shadow);
            return false;
        }

        if (strcmp(*name_slot, final_name) != 0) {
            if (!rename_scope_add(&scope, *name_slot, final_name)) {
                free(public_name);
                free(final_name);
                rename_scope_destroy(&scope);
                shadow_destroy(&shadow);
                return false;
            }
            free(*name_slot);
            *name_slot = pergyra_strdup(final_name);
        }

        free(public_name);
        free(final_name);
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode *stmt = statements[i];
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_NAMESPACE_DECL) {
            char *child_prefix =
                namespace_prefix_join(public_prefix, stmt->data.namespace_decl.name);
            bool child_export = inherited_export || stmt->is_exported;
            if (child_prefix == NULL
                || !normalize_statement_list(stmt->data.namespace_decl.statements,
                                             stmt->data.namespace_decl.count,
                                             child_prefix,
                                             private_prefix,
                                             imported,
                                             has_explicit_exports,
                                             child_export,
                                             &scope,
                                             flat)) {
                free(child_prefix);
                rename_scope_destroy(&scope);
                shadow_destroy(&shadow);
                return false;
            }
            free(child_prefix);
            free_namespace_shell(stmt);
            continue;
        }

        normalize_node_refs(stmt, &scope, &shadow);
        if (!astvec_push(flat, stmt)) {
            rename_scope_destroy(&scope);
            shadow_destroy(&shadow);
            return false;
        }
    }

    rename_scope_destroy(&scope);
    shadow_destroy(&shadow);
    return true;
}

static bool
normalize_module_ast(ASTNode *program, bool imported, const char *private_prefix)
{
    if (program == NULL || program->type != AST_PROGRAM)
        return false;

    ASTVec flat = {0};
    bool has_explicit = imported && module_has_explicit_exports(program);
    bool ok = normalize_statement_list(program->data.program.statements,
                                       program->data.program.count,
                                       "",
                                       private_prefix != NULL ? private_prefix : "",
                                       imported,
                                       has_explicit,
                                       false,
                                       NULL,
                                       &flat);
    if (!ok) {
        free(flat.items);
        return false;
    }

    free(program->data.program.statements);
    program->data.program.statements = flat.items;
    program->data.program.count = flat.count;
    return true;
}

static char *
replace_extension(const char *path, const char *new_ext)
{
    const char *dot = strrchr(path, '.');
    size_t base_len = dot ? (size_t)(dot - path) : strlen(path);
    size_t new_len = base_len + strlen(new_ext) + 1;
    char *result = malloc(new_len);
    if (result == NULL)
        return NULL;

    memcpy(result, path, base_len);
    strcpy(result + base_len, new_ext);
    return result;
}

static char *
default_binary_output_path(const char *source_path)
{
#ifdef _WIN32
    return replace_extension(source_path, ".exe");
#else
    return replace_extension(source_path, "");
#endif
}

static int
run_token_dump(const char *source, const char *path)
{
    Lexer *lexer = lexer_create(source);
    if (lexer == NULL) {
        fprintf(stderr, "pgy: lexer init failed for '%s'\n", path);
        return 1;
    }

    printf("=== tokens: %s ===\n", path);
    int n = 0;
    Token tok;
    do {
        tok = lexer_next_token(lexer);
        printf("%4d  ", ++n);
        token_print(&tok);
        if (tok.type == TOKEN_ERROR) {
            fprintf(stderr, "pgy: lex error: %s\n", lexer_get_error(lexer));
            lexer_destroy(lexer);
            return 1;
        }
    } while (tok.type != TOKEN_EOF);

    printf("  %d tokens total\n", n);
    lexer_destroy(lexer);
    return 0;
}

static int
run_pipeline(const DriverFlags *flags)
{
    char *source = read_file(flags->source_path);
    if (source == NULL)
        return 1;

    if (flags->dump_tokens) {
        int rc = run_token_dump(source, flags->source_path);
        free(source);
        return rc;
    }

    if (flags->verbose)
        printf("pgy: lexing %s\n", flags->source_path);

    Lexer *lexer = lexer_create(source);
    if (lexer == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        free(source);
        return 1;
    }

    if (flags->verbose)
        printf("pgy: parsing\n");

    Parser *parser = parser_create(lexer);
    if (parser == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    ASTNode *ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        fprintf(stderr, "pgy: parse error: %s\n", parser_get_error(parser));
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    if (!normalize_module_ast(ast, false, "")) {
        fprintf(stderr, "pgy: failed to normalize source module\n");
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    /* ---- Resolve imports: inline imported file ASTs ---- */
    {
        size_t import_module_counter = 0;
        /* Compute base directory from source_path */
        const char *last_sep = strrchr(flags->source_path, '/');
        const char *last_bsep = strrchr(flags->source_path, '\\');
        if (last_bsep != NULL && (last_sep == NULL || last_bsep > last_sep))
            last_sep = last_bsep;

        char base_dir[512] = ".";
        if (last_sep != NULL) {
            size_t dir_len = (size_t)(last_sep - flags->source_path);
            if (dir_len >= sizeof(base_dir)) dir_len = sizeof(base_dir) - 1;
            memcpy(base_dir, flags->source_path, dir_len);
            base_dir[dir_len] = '\0';
        }

        /* Scan for AST_IMPORT_DECL, parse imported files, merge statements */
        for (size_t i = 0; i < ast->data.program.count; i++) {
            ASTNode *stmt = ast->data.program.statements[i];
            if (stmt == NULL || stmt->type != AST_IMPORT_DECL)
                continue;

            const char *import_path = stmt->data.import_decl.path;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, import_path);

            char *imp_source = read_file(full_path);
            if (imp_source == NULL) {
                fprintf(stderr, "pgy: cannot resolve import '%s'\n", import_path);
                ast_destroy(ast);
                parser_destroy(parser);
                lexer_destroy(lexer);
                free(source);
                return 1;
            }

            Lexer *imp_lexer = lexer_create(imp_source);
            Parser *imp_parser = parser_create(imp_lexer);
            ASTNode *imp_ast = parser_parse_program(imp_parser);
            bool imp_err = parser_has_error(imp_parser);

            if (imp_err) {
                fprintf(stderr, "pgy: parse error in import '%s': %s\n",
                        import_path, parser_get_error(imp_parser));
                ast_destroy(imp_ast);
                parser_destroy(imp_parser);
                lexer_destroy(imp_lexer);
                free(imp_source);
                ast_destroy(ast);
                parser_destroy(parser);
                lexer_destroy(lexer);
                free(source);
                return 1;
            }

            {
                char private_prefix[64];
                snprintf(private_prefix, sizeof(private_prefix),
                         "__imp%zu_", import_module_counter++);
                if (!normalize_module_ast(imp_ast, true, private_prefix)) {
                    fprintf(stderr, "pgy: failed to normalize import '%s'\n", import_path);
                    ast_destroy(imp_ast);
                    parser_destroy(imp_parser);
                    lexer_destroy(imp_lexer);
                    free(imp_source);
                    ast_destroy(ast);
                    parser_destroy(parser);
                    lexer_destroy(lexer);
                    free(source);
                    return 1;
                }
            }

            /* Replace AST_IMPORT_DECL with imported statements */
            size_t imp_count = imp_ast->data.program.count;
            if (imp_count > 0) {
                size_t old_count = ast->data.program.count;
                size_t new_count = old_count - 1 + imp_count;
                ASTNode **new_stmts = malloc(new_count * sizeof(ASTNode*));
                /* Copy statements before import */
                for (size_t j = 0; j < i; j++)
                    new_stmts[j] = ast->data.program.statements[j];
                /* Copy imported statements */
                for (size_t j = 0; j < imp_count; j++)
                    new_stmts[i + j] = imp_ast->data.program.statements[j];
                /* Copy statements after import */
                for (size_t j = i + 1; j < old_count; j++)
                    new_stmts[j - 1 + imp_count] = ast->data.program.statements[j];

                ast_destroy(ast->data.program.statements[i]); /* free import node */
                free(ast->data.program.statements);
                ast->data.program.statements = new_stmts;
                ast->data.program.count = new_count;

                /* Detach imported statements from imp_ast so they aren't freed */
                imp_ast->data.program.statements = NULL;
                imp_ast->data.program.count = 0;

                /* Adjust index to skip newly inserted statements */
                i += imp_count - 1;
            } else {
                /* Empty import — just remove the import node */
                ast_destroy(ast->data.program.statements[i]);
                for (size_t j = i; j + 1 < ast->data.program.count; j++)
                    ast->data.program.statements[j] = ast->data.program.statements[j + 1];
                ast->data.program.count--;
                i--;
            }

            ast_destroy(imp_ast);
            parser_destroy(imp_parser);
            lexer_destroy(imp_lexer);
            free(imp_source);
        }
    }

    if (flags->dump_ast) {
        ast_print(ast, 0);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 0;
    }

    if (flags->verbose)
        printf("pgy: semantic analysis\n");

    SemanticResult *sem = semantic_analyze(ast);
    if (sem == NULL) {
        fprintf(stderr, "pgy: out of memory during semantic analysis\n");
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    semantic_result_print(sem);
    if (!sem->success) {
        fprintf(stderr, "pgy: %zu error(s) — aborting\n", sem->error_count);
        semantic_result_destroy(sem);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    char *hir_error = NULL;
    HIRProgram *hir = hir_lower(sem->annotated_ast, &hir_error);
    if (hir == NULL) {
        fprintf(stderr, "pgy: HIR lowering failed: %s\n",
                hir_error != NULL ? hir_error : "out of memory");
        free(hir_error);
        semantic_result_destroy(sem);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }
    free(hir_error);

    if (flags->dump_hir) {
        hir_dump(hir, stdout);
        hir_destroy(hir);
        semantic_result_destroy(sem);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 0;
    }

    int exit_code = 0;

#ifdef PGY_LLVM_ENABLED
    /* ---- LLVM backend ---- */
    if (flags->backend == BACKEND_LLVM && !flags->emit_c_only) {
        if (flags->emit_llvm_ir) {
            if (flags->verbose)
                printf("pgy: emitting LLVM IR\n");

            CompilerResult *result = flags->output_path != NULL
                ? compiler_emit_llvm_ir_to_file(hir,
                                                "pergyra_module",
                                                flags->output_path)
                : compiler_emit_llvm_ir(hir, "pergyra_module");
            if (result == NULL || !result->success) {
                fprintf(stderr, "pgy: LLVM IR generation failed: %s\n",
                        result != NULL ? result->error_message : "out of memory");
                compiler_result_destroy(result);
                hir_destroy(hir);
                semantic_result_destroy(sem);
                ast_destroy(ast);
                parser_destroy(parser);
                lexer_destroy(lexer);
                free(source);
                return 1;
            }

            if (flags->output_path != NULL)
                printf("pgy: wrote %s\n", flags->output_path);
            compiler_result_destroy(result);
        } else {
            char *bin_path = flags->output_path != NULL
                ? pergyra_strdup(flags->output_path)
                : default_binary_output_path(flags->source_path);
            char *obj_path = bin_path != NULL
                ? replace_extension(bin_path, ".o")
                : NULL;
            if (obj_path == NULL || bin_path == NULL) {
                fprintf(stderr, "pgy: out of memory\n");
                free(obj_path);
                free(bin_path);
                hir_destroy(hir);
                semantic_result_destroy(sem);
                ast_destroy(ast);
                parser_destroy(parser);
                lexer_destroy(lexer);
                free(source);
                return 1;
            }

            CompilerResult *result = compiler_build_native_llvm(
                hir, obj_path, bin_path, flags->verbose);
            if (result == NULL || !result->success) {
                fprintf(stderr, "pgy: LLVM compile failed: %s\n",
                        result != NULL ? result->error_message : "out of memory");
                compiler_result_destroy(result);
                free(obj_path);
                free(bin_path);
                hir_destroy(hir);
                semantic_result_destroy(sem);
                ast_destroy(ast);
                parser_destroy(parser);
                lexer_destroy(lexer);
                free(source);
                return 1;
            }

            printf("pgy: compiled (LLVM) → %s\n", bin_path);
            if (flags->do_run) {
                exit_code = compiler_run_binary(bin_path, flags->verbose);
                if (exit_code != 0)
                    fprintf(stderr, "pgy: program exited with code %d\n",
                            exit_code);
            }

            compiler_result_destroy(result);
            free(obj_path);
            free(bin_path);
        }
    } else
#endif /* PGY_LLVM_ENABLED */

    /* ---- C transpiler backend (default) ---- */
    if (flags->emit_c_only) {
        char *output_c = flags->output_path != NULL
            ? pergyra_strdup(flags->output_path)
            : replace_extension(flags->source_path, ".c");
        if (output_c == NULL) {
            fprintf(stderr, "pgy: out of memory\n");
            hir_destroy(hir);
            semantic_result_destroy(sem);
            ast_destroy(ast);
            parser_destroy(parser);
            lexer_destroy(lexer);
            free(source);
            return 1;
        }

        if (flags->verbose)
            printf("pgy: generating C → %s\n", output_c);

        CompilerResult *result = compiler_emit_c(hir, output_c);
        if (result == NULL || !result->success) {
            fprintf(stderr, "pgy: C generation failed: %s\n",
                    result != NULL ? result->error_message : "out of memory");
            compiler_result_destroy(result);
            free(output_c);
            hir_destroy(hir);
            semantic_result_destroy(sem);
            ast_destroy(ast);
            parser_destroy(parser);
            lexer_destroy(lexer);
            free(source);
            return 1;
        }

        printf("pgy: wrote %s\n", output_c);
        compiler_result_destroy(result);
        free(output_c);
    } else {
        /* Generate intermediate C in a temp directory (hidden from user).
         * The .c file is deleted after successful compilation. */
        char tmp_c[1024];
        {
            const char *tmpdir = getenv("TMPDIR");
            if (tmpdir == NULL) tmpdir = getenv("TMP");
            if (tmpdir == NULL) tmpdir = getenv("TEMP");
#ifdef _WIN32
            if (tmpdir == NULL) tmpdir = ".";
#else
            if (tmpdir == NULL) tmpdir = "/tmp";
#endif
            /* Extract base name from source path */
            const char *base = flags->source_path;
            const char *sep = strrchr(base, '/');
            if (sep != NULL) base = sep + 1;
#ifdef _WIN32
            sep = strrchr(base, '\\');
            if (sep != NULL) base = sep + 1;
#endif
            /* Strip extension */
            const char *dot = strrchr(base, '.');
            size_t blen = dot ? (size_t)(dot - base) : strlen(base);
            char stem[256];
            if (blen > sizeof(stem) - 1) blen = sizeof(stem) - 1;
            memcpy(stem, base, blen);
            stem[blen] = '\0';
            snprintf(tmp_c, sizeof(tmp_c), "%s/_pgy_%s_%u.c",
                     tmpdir, stem, (unsigned)getpid());
        }

        char *bin_path = flags->output_path != NULL
            ? pergyra_strdup(flags->output_path)
            : default_binary_output_path(flags->source_path);
        if (bin_path == NULL) {
            fprintf(stderr, "pgy: out of memory\n");
            hir_destroy(hir);
            semantic_result_destroy(sem);
            ast_destroy(ast);
            parser_destroy(parser);
            lexer_destroy(lexer);
            free(source);
            return 1;
        }

        if (flags->verbose)
            printf("pgy: generating C → %s\n", tmp_c);

        CompilerResult *result = compiler_build_native(hir,
                                                       tmp_c,
                                                       bin_path,
                                                       flags->verbose);

        /* Clean up intermediate C file */
        remove(tmp_c);

        if (result == NULL || !result->success) {
            fprintf(stderr, "pgy: compile failed: %s\n",
                    result != NULL ? result->error_message : "out of memory");
            compiler_result_destroy(result);
            free(bin_path);
            hir_destroy(hir);
            semantic_result_destroy(sem);
            ast_destroy(ast);
            parser_destroy(parser);
            lexer_destroy(lexer);
            free(source);
            return 1;
        }

        printf("pgy: compiled → %s\n", bin_path);
        if (flags->do_run) {
            exit_code = compiler_run_binary(bin_path, flags->verbose);
            if (exit_code != 0)
                fprintf(stderr, "pgy: program exited with code %d\n", exit_code);
        }

        compiler_result_destroy(result);
        free(bin_path);
    }

    hir_destroy(hir);
    semantic_result_destroy(sem);
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    free(source);
    return exit_code;
}

static void
print_usage(void)
{
    printf(
        "Usage:\n"
        "  pgy <source.pgy>              compile to native binary\n"
        "  pgy <source.pgy> -o <out>     name the emitted native binary\n"
        "  pgy <source.pgy> --emit-c     stop after generating C\n"
        "  pgy <source.pgy> --emit-c -o <out.c>\n"
        "  pgy <source.pgy> --emit-llvm -o <out.ll>\n"
        "  pgy <source.pgy> --run        compile + run\n"
        "  pgy --tokens <source.pgy>     dump token stream\n"
        "  pgy --ast    <source.pgy>     dump AST\n"
        "  pgy --hir    <source.pgy>     dump lowered HIR summary\n"
#ifdef PGY_LLVM_ENABLED
        "  default backend: LLVM\n"
        "  pgy <source.pgy> --backend=llvm   use LLVM native backend\n"
#else
        "  default backend: C\n"
#endif
#ifdef PGY_LLVM_ENABLED
        "  pgy <source.pgy> --emit-llvm      emit LLVM IR text\n"
#endif
        "  pgy --repl                    interactive REPL\n"
        "  pgy --help\n");
}

static DriverFlags
parse_args(int argc, char *argv[])
{
    DriverFlags f;
    memset(&f, 0, sizeof(f));
#ifdef PGY_LLVM_ENABLED
    f.backend = BACKEND_LLVM;
#else
    f.backend = BACKEND_C;
#endif

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            exit(0);
        } else if (strcmp(argv[i], "--compile") == 0) {
            continue;
        } else if (strcmp(argv[i], "--emit-c") == 0) {
            f.emit_c_only = true;
            f.backend = BACKEND_C;
        } else if (strcmp(argv[i], "--backend=llvm") == 0) {
            f.backend = BACKEND_LLVM;
        } else if (strcmp(argv[i], "--backend=c") == 0) {
            f.backend = BACKEND_C;
        } else if (strcmp(argv[i], "--emit-llvm") == 0) {
            f.emit_llvm_ir = true;
            f.backend = BACKEND_LLVM;
        } else if (strcmp(argv[i], "--run") == 0) {
            f.do_run = true;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            f.dump_tokens = true;
        } else if (strcmp(argv[i], "--ast") == 0) {
            f.dump_ast = true;
        } else if (strcmp(argv[i], "--repl") == 0) {
            f.repl = true;
        } else if (strcmp(argv[i], "--hir") == 0) {
            f.dump_hir = true;
        } else if (strcmp(argv[i], "-v") == 0
                || strcmp(argv[i], "--verbose") == 0) {
            f.verbose = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "pgy: -o requires an argument\n");
                exit(1);
            }
            f.output_path = argv[++i];
        } else if (argv[i][0] != '-') {
            f.source_path = argv[i];
        } else {
            fprintf(stderr, "pgy: unknown option '%s'\n", argv[i]);
            exit(1);
        }
    }

    if (f.source_path == NULL && !f.repl) {
        print_usage();
        exit(1);
    }

#ifndef PGY_LLVM_ENABLED
    if (f.backend == BACKEND_LLVM || f.emit_llvm_ir) {
        fprintf(stderr, "pgy: this build was compiled without LLVM backend support\n");
        exit(1);
    }
#endif

    return f;
}

/* Generate a unique temp file path in TMPDIR (or /tmp fallback) */
static void
repl_tmp_path(char *out, size_t out_size, const char *ext)
{
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL) tmpdir = getenv("TMP");
    if (tmpdir == NULL) tmpdir = getenv("TEMP");
#ifdef _WIN32
    if (tmpdir == NULL) tmpdir = ".";
#else
    if (tmpdir == NULL) tmpdir = "/tmp";
#endif
    static unsigned repl_salt = 0;
    if (repl_salt == 0) {
        repl_salt = (unsigned)time(NULL) ^ (unsigned)getpid();
    }
    snprintf(out, out_size, "%s/pgy_repl_%u_%x%s",
             tmpdir, (unsigned)getpid(), repl_salt, ext);
}

static int
run_repl(void)
{
    printf("Pergyra REPL v0.1 — type 'exit' to quit\n");

    /* Accumulate top-level declarations (func, struct, etc.) */
    char decls[16384] = "";
    char line[2048];

    while (1) {
        printf("pgy> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        /* Strip trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';
        if (len > 0 && line[len - 1] == '\r')
            line[--len] = '\0';

        if (len == 0)
            continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)
            break;

        /* If line starts with func/struct/class/ability/role, accumulate */
        bool is_decl = (strncmp(line, "func ", 5) == 0
                     || strncmp(line, "struct ", 7) == 0
                     || strncmp(line, "class ", 6) == 0);

        if (is_decl) {
            /* Read until closing brace by counting {} */
            char block[4096];
            snprintf(block, sizeof(block), "%s\n", line);
            int depth = 0;
            for (const char *p = line; *p; p++) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
            }
            while (depth > 0) {
                printf("...  ");
                fflush(stdout);
                if (fgets(line, sizeof(line), stdin) == NULL)
                    break;
                size_t l = strlen(line);
                if (l > 0 && line[l - 1] == '\n') line[--l] = '\0';
                strncat(block, line, sizeof(block) - strlen(block) - 2);
                strcat(block, "\n");
                for (const char *p = line; *p; p++) {
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                }
            }
            strncat(decls, block, sizeof(decls) - strlen(decls) - 1);
            printf("  (defined)\n");
            continue;
        }

        /* Build temp source: decls + func Main() { <line> } */
        char tmp_source[32768];
        snprintf(tmp_source, sizeof(tmp_source),
            "%s\nfunc Main() -> Void {\n    %s\n}\n", decls, line);

        /* Write to unique temp file */
        char tmp_pgy[512], tmp_c[512], tmp_exe[512];
        repl_tmp_path(tmp_pgy, sizeof(tmp_pgy), ".pgy");
        repl_tmp_path(tmp_c,   sizeof(tmp_c),   ".c");
        repl_tmp_path(tmp_exe, sizeof(tmp_exe),  ".exe");

        FILE *f = fopen(tmp_pgy, "w");
        if (f == NULL) {
            fprintf(stderr, "  error: cannot create temp file\n");
            continue;
        }
        fputs(tmp_source, f);
        fclose(f);

        /* Use this driver itself to compile+run */
        DriverFlags rf;
        memset(&rf, 0, sizeof(rf));
        rf.source_path = tmp_pgy;
        rf.do_run = true;
        run_pipeline(&rf);

        /* Cleanup temp files */
        remove(tmp_pgy);
        remove(tmp_c);
        remove(tmp_exe);
    }

    printf("Bye!\n");
    return 0;
}

int
main(int argc, char *argv[])
{
    DriverFlags flags = parse_args(argc, argv);
    if (flags.repl)
        return run_repl();
    return run_pipeline(&flags);
}
