/* String loans live across an inout call whose callee may free an element
 * of the storage they borrow; see indexed_string_borrow_owner.h. Edges and
 * reads name bindings by SyntaxNodeId; AST pointers are the checked
 * program's and outlive the semantic context. */

#include <stdlib.h>
#include <string.h>

#include "call_argument_move_order_owner.h"
#include "diag_codes.h"
#include "indexed_string_borrow_owner.h"
#include "indexed_string_definition_recipe.h"
#include "loan_copy_landing_owner.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "type_checker/type_checker_internal.h"

#ifndef INDEXED_STRING_BORROW_UNKNOWN_SOURCE
#define INDEXED_STRING_BORROW_UNKNOWN_SOURCE UINT32_MAX
#endif

/* One inout call a loan was live across, or (call NULL) the join of two
 * branches' edges. `next` and `join` are 1-based indexes of earlier edges. */
typedef struct IndexedStringCallReleaseEdge {
    ASTNode *call;
    uint32_t call_id;
    size_t actual;
    uint32_t next, join;
} IndexedStringCallReleaseEdge;

/* A read of a local that holds edges, with the edges it held there. */
typedef struct IndexedStringCallReleaseRead {
    ASTNode *use;
    ASTNode *routine;
    const char *name;
    uint32_t use_id, binding_id, source_id, head;
} IndexedStringCallReleaseRead;

/* A value of this type can hold String elements an inout callee frees. */
static bool
call_release_type(const Type *type)
{
    return type != NULL && type != TYPE_UNKNOWN && !type_equals((Type *)type, TYPE_STRING)
        && (type->kind == TYPE_KIND_CLASS || type->kind == TYPE_KIND_CONSTRUCTED
            || type->kind == TYPE_KIND_TUPLE || type->kind == TYPE_KIND_ENUM);
}

static uint32_t
call_release_edge_add(SemanticContext *ctx, IndexedStringCallReleaseEdge edge, ASTNode *site)
{
    if (ctx->indexed_string_call_release_edge_count >= UINT32_MAX - 1) {
        semantic_error(ctx, site, "String call-release edge count overflow");
        return 0;
    }
    if (ctx->indexed_string_call_release_edge_count == ctx->indexed_string_call_release_edge_capacity) {
        size_t capacity = ctx->indexed_string_call_release_edge_capacity == 0
            ? 64 : ctx->indexed_string_call_release_edge_capacity * 2;
        IndexedStringCallReleaseEdge *rows = realloc(ctx->indexed_string_call_release_edges,
            capacity * sizeof(*rows));
        if (rows == NULL) {
            semantic_error(ctx, site, "String call-release edge allocation failed");
            return 0;
        }
        ctx->indexed_string_call_release_edges = rows;
        ctx->indexed_string_call_release_edge_capacity = capacity;
    }
    ctx->indexed_string_call_release_edges[ctx->indexed_string_call_release_edge_count++] = edge;
    return (uint32_t)ctx->indexed_string_call_release_edge_count;
}

void
semantic_indexed_string_borrow_record_call_release(ASTNode *argument,
    const Type *type, ASTNode *call, size_t actual, SemanticContext *ctx)
{
    if (ctx == NULL || argument == NULL || call == NULL || ast_node_stable_id(call) == 0
        || ctx->current_function_decl == NULL
        || ctx->current_function_decl->type != AST_FUNC_DECL
        || !semantic_param_publication_pending(ctx) || !call_release_type(type))
        return;
    /* An actual that names an Array<String> binding has an exact storage
     * source; any other actual reaches only loans of unknown source. */
    bool tracked = false;
    uint32_t source_id = 0;
    if (!semantic_indexed_string_borrow_array_receiver_source(argument, ctx, &tracked, &source_id)
        || !tracked)
        source_id = 0;
    for (Scope *scope = ctx->scope; scope != NULL;
            scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *loan = scope->symbols[i];
            uint32_t loan_source = loan != NULL ? loan->indexed_string_borrow_source_syntax_id : 0;
            if (loan == NULL || !type_equals(loan->type, TYPE_STRING)
                || loan->indexed_string_borrow_invalidated || loan_source == 0
                || (loan_source != INDEXED_STRING_BORROW_UNKNOWN_SOURCE && loan_source != source_id))
                continue;
            uint32_t head = loan->indexed_string_call_release_head;
            const IndexedStringCallReleaseEdge *last = head != 0
                ? &ctx->indexed_string_call_release_edges[head - 1] : NULL;
            if (last != NULL && last->call == call && last->actual == actual)
                continue;
            uint32_t added = call_release_edge_add(ctx, (IndexedStringCallReleaseEdge){
                call, ast_node_stable_id(call), actual, head, 0}, argument);
            if (added == 0)
                return;
            loan->indexed_string_call_release_head = added;
        }
    }
}

bool
semantic_indexed_string_borrow_record_call_release_read(Symbol *binding,
    ASTNode *use, SemanticContext *ctx)
{
    /* Inside a loop a loan read before any call may still be behind a call
     * later in the body, through the back edge. */
    if (ctx == NULL || binding == NULL || use == NULL
        || binding->indexed_string_borrow_source_syntax_id == 0
        || (binding->indexed_string_call_release_head == 0 && ctx->loop_depth <= 0)
        || !semantic_param_publication_pending(ctx))
        return true;
    if (ast_node_stable_id(use) == 0 || binding->decl_syntax_id == 0
        || ctx->current_function_decl == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, use,
            "A String read after an inout call has no stable identity");
        return false;
    }
    if (ctx->indexed_string_call_release_read_count == ctx->indexed_string_call_release_read_capacity) {
        size_t capacity = ctx->indexed_string_call_release_read_capacity == 0
            ? 64 : ctx->indexed_string_call_release_read_capacity * 2;
        IndexedStringCallReleaseRead *rows = realloc(ctx->indexed_string_call_release_reads,
            capacity * sizeof(*rows));
        if (rows == NULL) {
            semantic_error(ctx, use, "String call-release read allocation failed");
            return false;
        }
        ctx->indexed_string_call_release_reads = rows;
        ctx->indexed_string_call_release_read_capacity = capacity;
    }
    /* The name is read from the syntax tree, which outlives the scope. */
    const char *name = use->type == AST_IDENTIFIER ? ast_identifier_name(use) : NULL;
    ctx->indexed_string_call_release_reads[ctx->indexed_string_call_release_read_count++] =
        (IndexedStringCallReleaseRead){use, ctx->current_function_decl, name,
            ast_node_stable_id(use), binding->decl_syntax_id,
            binding->indexed_string_borrow_source_syntax_id,
            binding->indexed_string_call_release_head};
    return true;
}

uint32_t
semantic_indexed_string_borrow_call_release_join(SemanticContext *ctx,
    uint32_t base, uint32_t joined, uint32_t incoming)
{
    /* Each branch starts from `base` and only adds edges in front of it. */
    if (joined == UINT32_MAX || joined == base)
        return incoming;
    if (incoming == base || incoming == joined)
        return joined;
    uint32_t added = ctx != NULL ? call_release_edge_add(ctx,
        (IndexedStringCallReleaseEdge){NULL, 0, 0, joined, incoming},
        ctx->current_function_decl) : 0;
    /* On allocation failure keep the incoming edges: the error already
     * refuses the program. */
    return added != 0 ? added : incoming;
}

size_t
semantic_indexed_string_borrow_call_release_loop_begin(const SemanticContext *ctx)
{
    return ctx != NULL ? ctx->indexed_string_call_release_read_count : 0;
}

bool
semantic_indexed_string_borrow_call_release_continue(SemanticContext *ctx,
    const Scope *outer, IndexedStringCallReleaseHead **heads, size_t *count,
    size_t *capacity)
{
    if (ctx == NULL || heads == NULL || count == NULL || capacity == NULL)
        return true;
    for (const Scope *scope = outer; scope != NULL;
            scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            const Symbol *loan = scope->symbols[i];
            if (loan == NULL || loan->indexed_string_call_release_head == 0
                || loan->decl_syntax_id == 0)
                continue;
            if (*count == *capacity) {
                size_t grown_capacity = *capacity == 0 ? 8 : *capacity * 2;
                IndexedStringCallReleaseHead *grown = realloc(*heads,
                    grown_capacity * sizeof(*grown));
                if (grown == NULL) {
                    semantic_error(ctx, ctx->current_function_decl,
                        "String call-release continue record allocation failed");
                    return false;
                }
                *heads = grown;
                *capacity = grown_capacity;
            }
            (*heads)[(*count)++] = (IndexedStringCallReleaseHead){
                loan->decl_syntax_id, loan->indexed_string_call_release_head};
        }
    }
    return true;
}

/* Join `head` into each read of `binding` since `reads_from`. */
static bool
call_release_back_edge_join(SemanticContext *ctx, size_t reads_from,
    uint32_t binding, uint32_t head)
{
    for (size_t r = reads_from; r < ctx->indexed_string_call_release_read_count; r++) {
        IndexedStringCallReleaseRead *read = &ctx->indexed_string_call_release_reads[r];
        if (read->binding_id != binding || read->head == head)
            continue;
        if (read->head == 0) {
            read->head = head;
            continue;
        }
        uint32_t joined = call_release_edge_add(ctx,
            (IndexedStringCallReleaseEdge){NULL, 0, 0, read->head, head}, read->use);
        if (joined == 0)
            return false;
        read->head = joined;
    }
    return true;
}

void
semantic_indexed_string_borrow_call_release_back_edge(SemanticContext *ctx,
    const Scope *outer, size_t reads_from,
    const IndexedStringCallReleaseHead *continues, size_t continue_count)
{
    if (ctx == NULL || reads_from >= ctx->indexed_string_call_release_read_count)
        return;
    for (const Scope *scope = outer; scope != NULL;
            scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            const Symbol *loan = scope->symbols[i];
            uint32_t end = loan != NULL ? loan->indexed_string_call_release_head : 0;
            if (end != 0 && loan->decl_syntax_id != 0
                && !call_release_back_edge_join(ctx, reads_from, loan->decl_syntax_id, end))
                return;
        }
    }
    for (size_t c = 0; continues != NULL && c < continue_count; c++)
        if (!call_release_back_edge_join(ctx, reads_from, continues[c].binding_id,
                continues[c].head))
            return;
}

/* Whether the callee of this edge's call may free an element of the
 * storage its actual names. A call whose callee or formal is not published
 * may. */
static bool
call_release_edge_frees(SemanticContext *ctx, const IndexedStringCallReleaseEdge *edge,
    ASTNode **callee_out)
{
    *callee_out = NULL;
    if (edge->call == NULL || ast_node_stable_id(edge->call) != edge->call_id)
        return true;
    ASTNode *callee = semantic_param_publication_callee(ctx, edge->call, edge->actual);
    size_t formal = 0;
    if (callee == NULL || callee->type != AST_FUNC_DECL
        || !semantic_param_publication_formal_for_actual(ctx, edge->call, edge->actual, &formal))
        return true;
    *callee_out = callee;
    return semantic_formal_release_frees_elements(ctx, callee, formal);
}

static void
call_release_refuse(SemanticContext *ctx, const IndexedStringCallReleaseRead *read,
    const ASTNode *callee)
{
    const char *name = read->name != NULL ? read->name : "<string>";
    const char *routine = callee != NULL && ast_declaration_name(callee) != NULL
        ? ast_declaration_name(callee) : "<callee>";
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, read->use,
        "String '%s' borrows an Array or List element that a call to '%s' may replace or release.\n"
        "Reason:\n"
        "- '%s' frees a String element of the storage passed to its inout parameter (an owned ArraySet or ArrayPop, ArrayDropOwnedStrings, ListSet or ListRemove, there or in a routine it calls)\n"
        "- reading the alias after the call would read freed memory\n"
        "Fix:\n"
        "- read the String before the call\n"
        "- or copy it with Concat(\"\", value) before the call",
        name, routine, routine);
}

bool
semantic_indexed_string_borrow_call_release_receipts(SemanticContext *ctx)
{
    size_t edge_count = ctx != NULL ? ctx->indexed_string_call_release_edge_count : 0;
    if (ctx == NULL || ctx->indexed_string_call_release_read_count == 0)
        return true;
    /* Per edge, the first call reachable through it whose callee may free
     * the element (1-based), and that callee. Edges only name earlier ones. */
    uint32_t *retiring = calloc(edge_count != 0 ? edge_count : 1, sizeof(*retiring));
    ASTNode **callees = calloc(edge_count != 0 ? edge_count : 1, sizeof(*callees));
    if (retiring == NULL || callees == NULL) {
        free(retiring);
        free(callees);
        semantic_error(ctx, ctx->program_root, "String call-release verdict allocation failed");
        return false;
    }
    for (size_t e = 0; e < edge_count; e++) {
        const IndexedStringCallReleaseEdge *edge = &ctx->indexed_string_call_release_edges[e];
        ASTNode *callee = NULL;
        if (edge->call != NULL && call_release_edge_frees(ctx, edge, &callee)) {
            retiring[e] = (uint32_t)e + 1;
            callees[e] = callee;
            continue;
        }
        uint32_t from = edge->next != 0 && retiring[edge->next - 1] != 0 ? edge->next
            : edge->join != 0 && retiring[edge->join - 1] != 0 ? edge->join : 0;
        if (from != 0) {
            retiring[e] = retiring[from - 1];
            callees[e] = callees[from - 1];
        }
    }
    /* The first retired read of each binding (one copy or diagnostic each). */
    size_t *firsts = NULL;
    size_t first_count = 0;
    bool ok = true;
    for (size_t r = 0; ok && r < ctx->indexed_string_call_release_read_count; r++) {
        const IndexedStringCallReleaseRead *read = &ctx->indexed_string_call_release_reads[r];
        if (read->head == 0)
            continue;
        if (read->head > edge_count || ast_node_stable_id(read->use) != read->use_id) {
            semantic_error(ctx, read->use, "String call-release read identity is no longer current");
            ok = false;
            break;
        }
        if (retiring[read->head - 1] == 0)
            continue;
        bool handled = false;
        for (size_t p = 0; p < first_count && !handled; p++)
            handled = ctx->indexed_string_call_release_reads[firsts[p]].binding_id == read->binding_id;
        if (handled)
            continue;
        size_t *grown = realloc(firsts, (first_count + 1) * sizeof(*firsts));
        if (grown == NULL) {
            semantic_error(ctx, read->use, "String call-release verdict allocation failed");
            ok = false;
            break;
        }
        firsts = grown;
        firsts[first_count++] = r;
    }
    /* Copies first: a refusal emitted before them would withdraw the
     * definition owner's premise for the rest. */
    bool *selected = ok && first_count != 0 ? calloc(first_count, sizeof(*selected)) : NULL;
    if (ok && first_count != 0 && selected == NULL) {
        semantic_error(ctx, ctx->program_root, "String call-release verdict allocation failed");
        ok = false;
    }
    ASTNode *saved = ctx->current_function_decl;
    for (size_t f = 0; ok && f < first_count; f++) {
        const IndexedStringCallReleaseRead *read = &ctx->indexed_string_call_release_reads[firsts[f]];
        if (read->source_id != INDEXED_STRING_BORROW_UNKNOWN_SOURCE)
            continue;
        ctx->current_function_decl = read->routine;
        selected[f] = semantic_indexed_string_definition_select_independent_storage(
            ctx, read->binding_id);
        ctx->current_function_decl = saved;
        if (selected[f])
            semantic_loan_copy_landing_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, read->use,
                "A String loan read after a call that may release its element is copied at each reaching definition");
    }
    for (size_t f = 0; ok && f < first_count; f++) {
        if (!selected[f])
            call_release_refuse(ctx, &ctx->indexed_string_call_release_reads[firsts[f]],
                callees[ctx->indexed_string_call_release_reads[firsts[f]].head - 1]);
    }
    ctx->current_function_decl = saved;
    if (first_count != 0)
        ok = false;
    free(selected);
    free(firsts);
    free(retiring);
    free(callees);
    return ok;
}
