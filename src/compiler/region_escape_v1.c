#include "region_escape_v1.h"

#include <stdlib.h>

#include "../parser/ast.h"
#include "../parser/ast_api.h"
#include "../lexer/lexer.h" /* TOKEN_PLUS */
#include "../semantic/builtin_kind.h"

/*
 * Structural traversal is intentionally narrow and uses stable syntax facts;
 * callee authority comes from the semantic builtin-kind annotation through the
 * AST API. The producer never recovers a borrow-safe callee from source text.
 */

typedef struct {
    PgyRegionEscapeSite *sites;
    size_t               count;
    size_t               cap;
    uint32_t             next_scope; /* function-scope id allocator */
    bool                 oom;
    bool                 invalid_site;
} EscapeCollector;

static void
escape_push(EscapeCollector *c,
            const ASTNode *site,
            uint32_t scope,
            uint32_t function_syntax_id)
{
    if (c->oom)
        return;
    if (c->count == c->cap) {
        size_t ncap = c->cap ? c->cap * 2 : 8;
        PgyRegionEscapeSite *grown = realloc(c->sites, ncap * sizeof(*grown));
        if (grown == NULL) {
            /* OOM during analysis is conservative: stop certifying (every
               uncertified site stays HEAP). Never a soundness hazard. */
            c->oom = true;
            return;
        }
        c->sites = grown;
        c->cap = ncap;
    }
    PgyRegionAllocationSiteId allocation_site_id =
        ast_node_stable_id(site);
    if (allocation_site_id == 0) {
        c->invalid_site = true;
        return;
    }
    c->sites[c->count].allocation_site_id = allocation_site_id;
    c->sites[c->count].scope_id = scope;
    c->sites[c->count].function_syntax_id = function_syntax_id;
    c->count++;
}

/* A string-concat expression: `+` where an operand is a string literal, or the
   left operand is itself a string-concat (so a literal-rooted chain a+b+c is
   recognised end to end). Type-directed detection would widen this to
   String-typed variables; v1 stays with the syntactic, obviously-sound chain. */
static bool
is_string_concat(const ASTNode *e)
{
    const ASTNode *l;
    const ASTNode *r;
    if (e == NULL || e->type != AST_BINARY)
        return false;
    if (e->data.binary.op.type != TOKEN_PLUS)
        return false;
    l = e->data.binary.left;
    r = e->data.binary.right;
    if (l != NULL && l->type == AST_STRING)
        return true;
    if (r != NULL && r->type == AST_STRING)
        return true;
    return is_string_concat(l);
}

/* Certify a concat node and its left-spine nested concats: a+b+c parses as
   ((a+b)+c), so the argument node and its left child (a+b) are both concat
   sites the emitter will lower. */
static void
certify_concat_spine(EscapeCollector *c,
                     const ASTNode *e,
                     uint32_t scope,
                     uint32_t function_syntax_id)
{
    while (is_string_concat(e)) {
        escape_push(c, e, scope, function_syntax_id);
        e = e->data.binary.left;
    }
}

static bool
is_print_call(const ASTNode *call)
{
    uint32_t builtin_kind = 0;
    if (call == NULL || call->type != AST_CALL)
        return false;
    return ast_call_semantic_callee_builtin_kind(call, &builtin_kind)
        && builtin_kind == (uint32_t)BUILTIN_PRINT;
}

static void
escape_walk(EscapeCollector *c,
            const ASTNode *node,
            uint32_t scope,
            uint32_t function_syntax_id)
{
    if (node == NULL || c->oom)
        return;

    switch (node->type) {
    case AST_PROGRAM: {
        for (size_t i = 0; i < node->data.program.count; i++)
            escape_walk(c, node->data.program.statements[i], scope,
                        function_syntax_id);
        break;
    }
    case AST_FUNC_DECL: {
        /* Each function body is its own region scope. */
        uint32_t fn_scope = ++c->next_scope;
        uint32_t fn_syntax_id = ast_node_stable_id(node);
        escape_walk(c, node->data.func_decl.body, fn_scope, fn_syntax_id);
        break;
    }
    case AST_BLOCK: {
        for (size_t i = 0; i < node->data.block.count; i++)
            escape_walk(c, node->data.block.statements[i], scope,
                        function_syntax_id);
        break;
    }
    case AST_CALL: {
        if (is_print_call(node)) {
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                const ASTNode *arg = node->data.call.arguments[i];
                if (is_string_concat(arg))
                    certify_concat_spine(c, arg, scope, function_syntax_id);
            }
        }
        /* Descend into arguments so a Print nested in another call's args is
           still reached (the nested Print's own concat args get certified). */
        for (size_t i = 0; i < node->data.call.arg_count; i++)
            escape_walk(c, node->data.call.arguments[i], scope,
                        function_syntax_id);
        break;
    }
    default:
        /* Sound incompleteness: any container not handled here simply yields no
           certifications for the sites it holds, which keeps them HEAP. */
        break;
    }
}

size_t
pgy_region_escape_v1_collect(const struct ASTNode *root,
                             PgyRegionEscapeSite **sites_out)
{
    EscapeCollector c;
    c.sites = NULL;
    c.count = 0;
    c.cap = 0;
    c.next_scope = 0;
    c.oom = false;
    c.invalid_site = false;

    escape_walk(&c, root, 0, 0);

    if (c.oom || c.invalid_site) {
        /* Hand back the honest conservative answer (all HEAP) rather than a
           silent truncation. */
        free(c.sites);
        c.sites = NULL;
        c.count = 0;
    }
    if (sites_out != NULL)
        *sites_out = c.sites;
    else
        free(c.sites);
    return c.count;
}

void
pgy_region_escape_v1_free(PgyRegionEscapeSite *sites)
{
    free(sites);
}
