#include "region_escape_v1.h"

#include <stdlib.h>
#include <string.h>

#include "../parser/ast.h"
#include "../lexer/lexer.h" /* TOKEN_PLUS */

/*
 * Direct AST field access (like the codegen emitters, e.g. emit_binary reading
 * node->type) rather than the ast_api accessors, so this pass is self-contained
 * -- it needs only the ASTNode struct definition, not the parser object. That
 * keeps the analysis unit-testable against hand-built nodes with no parser link.
 */

typedef struct {
    PgyRegionEscapeSite *sites;
    size_t               count;
    size_t               cap;
    uint32_t             next_scope; /* function-scope id allocator */
    bool                 oom;
} EscapeCollector;

static void
escape_push(EscapeCollector *c, const ASTNode *site, uint32_t scope)
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
    c->sites[c->count].site = site;
    c->sites[c->count].scope_id = scope;
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
certify_concat_spine(EscapeCollector *c, const ASTNode *e, uint32_t scope)
{
    while (is_string_concat(e)) {
        escape_push(c, e, scope);
        e = e->data.binary.left;
    }
}

static bool
is_print_call(const ASTNode *call)
{
    const ASTNode *callee;
    const char *name;
    if (call == NULL || call->type != AST_CALL)
        return false;
    callee = call->data.call.callee;
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    name = callee->data.identifier.name;
    return name != NULL
        && (strcmp(name, "Print") == 0 || strcmp(name, "PrintLn") == 0);
}

static void
escape_walk(EscapeCollector *c, const ASTNode *node, uint32_t scope)
{
    if (node == NULL || c->oom)
        return;

    switch (node->type) {
    case AST_PROGRAM: {
        for (size_t i = 0; i < node->data.program.count; i++)
            escape_walk(c, node->data.program.statements[i], scope);
        break;
    }
    case AST_FUNC_DECL: {
        /* Each function body is its own region scope. */
        uint32_t fn_scope = ++c->next_scope;
        escape_walk(c, node->data.func_decl.body, fn_scope);
        break;
    }
    case AST_BLOCK: {
        for (size_t i = 0; i < node->data.block.count; i++)
            escape_walk(c, node->data.block.statements[i], scope);
        break;
    }
    case AST_CALL: {
        if (is_print_call(node)) {
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                const ASTNode *arg = node->data.call.arguments[i];
                if (is_string_concat(arg))
                    certify_concat_spine(c, arg, scope);
            }
        }
        /* Descend into arguments so a Print nested in another call's args is
           still reached (the nested Print's own concat args get certified). */
        for (size_t i = 0; i < node->data.call.arg_count; i++)
            escape_walk(c, node->data.call.arguments[i], scope);
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

    escape_walk(&c, root, 0);

    if (c.oom) {
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
