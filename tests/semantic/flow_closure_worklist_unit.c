/* Which entries a pass of a solving parameter-flow closure evaluates again,
 * on forged store rows around parsed routines. A change of an entry marks
 * every evaluated closure entry of a routine that read it, and every fact a
 * reader can see counts as a change. The flow summary gate covers real
 * closures, checked against a full pass with PGY_VERIFY_FLOW_WORKLIST=1. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/type_checker.h"
#include "semantic/function_param_flow/function_param_flow_summary_internal.h"

static unsigned checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { failures++; fprintf(stderr, "closure worklist fault line %d: %s\n", __LINE__, #x); } } while (0)

static FunctionParamFlowSummaryStore store;

static void clear_marks(void)
{
    for (size_t i = 0; i < store.count; i++) store.entries[i].dirty = false;
}

static unsigned marked(void)
{
    unsigned bits = 0;
    for (size_t i = 0; i < store.count; i++) if (store.entries[i].dirty) bits |= 1u << i;
    return bits;
}

/* A copy of `base` with its own outputs, changed by `change`, must differ. */
#define CHANGED(change) do { \
        FunctionParamFlowSummaryEntry c = base; bool copied[2] = {outputs[0], outputs[1]}; \
        c.output_dependencies = copied; change; \
        CHECK(!function_param_flow_entry_facts_equal(facts, function_param_flow_entry_facts(&c))); \
    } while (0)

int main(void)
{
    Lexer *lexer = lexer_create("func Reader(a: Int, b: Int) -> Int { return Read(a); }\n"
        "func Read(a: Int) -> Int { return a; }\n"
        "func Other(a: Int) -> Int { return a; }\n");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(ast_assign_stable_ids(program));
    ASTNode *reader = ast_program_statement(program, 0);
    ASTNode *read = ast_program_statement(program, 1);
    ASTNode *other = ast_program_statement(program, 2);
    CHECK(reader != NULL && read != NULL && other != NULL && ast_func_param_count(reader) == 2);
    if (failures != 0) return 1;

    store.solving = true;
    size_t r0 = function_param_flow_add(&store, reader, 0);
    size_t r1 = function_param_flow_add(&store, reader, 1);
    size_t d = function_param_flow_add(&store, read, 0);
    size_t o = function_param_flow_add(&store, other, 0);
    CHECK(r0 == 0 && r1 == 1 && d == 2 && o == 3);
    if (failures != 0) return 1;
    for (size_t i = 0; i < store.count; i++) store.entries[i].state = FUNCTION_PARAM_FLOW_EVALUATED;

    /* A read outside an evaluation has no reader. */
    CHECK(function_param_flow_closure_read(&store, d) && store.closure_read_count == 0);
    /* Reader's first entry reads Read's entry; a second read adds nothing. */
    store.evaluating_plus_one = r0 + 1;
    CHECK(function_param_flow_closure_read(&store, d) && store.closure_read_count == 1);
    CHECK(function_param_flow_closure_read(&store, d) && store.closure_read_count == 1);
    store.evaluating_plus_one = 0;

    /* A change of Read marks every entry of Reader, and nothing else. */
    function_param_flow_closure_changed(&store, d);
    CHECK(marked() == ((1u << r0) | (1u << r1)));
    /* A change of an entry nobody read marks nothing. */
    clear_marks();
    function_param_flow_closure_changed(&store, o);
    CHECK(marked() == 0);
    /* An entry this pass has not evaluated yet reads the change when it is. */
    store.entries[r1].state = FUNCTION_PARAM_FLOW_UNSEEN;
    function_param_flow_closure_changed(&store, d);
    CHECK(marked() == (1u << r0));
    clear_marks();
    /* An entry still being evaluated read the old value. */
    store.entries[r1].state = FUNCTION_PARAM_FLOW_COMPUTING;
    function_param_flow_closure_changed(&store, d);
    CHECK(marked() == ((1u << r0) | (1u << r1)));
    clear_marks();
    store.entries[r1].state = FUNCTION_PARAM_FLOW_EVALUATED;

    /* Entries before the solving closure are completed: not marked, not read. */
    store.active_start = r1;
    function_param_flow_closure_changed(&store, d);
    CHECK(marked() == (1u << r1));
    clear_marks();
    store.evaluating_plus_one = o + 1;
    CHECK(function_param_flow_closure_read(&store, r0) && store.closure_read_count == 1);
    function_param_flow_closure_changed(&store, r0);
    CHECK(marked() == 0);
    /* A completed entry's list belongs to an earlier closure. */
    store.entries[r0].readers_plus_one = 1;
    function_param_flow_closure_changed(&store, r0);
    CHECK(marked() == 0);
    store.entries[r0].readers_plus_one = 0;
    /* Other reads Reader's second entry; its change marks Other only. */
    CHECK(function_param_flow_closure_read(&store, r1) && store.closure_read_count == 2);
    store.evaluating_plus_one = 0;
    function_param_flow_closure_changed(&store, r1);
    CHECK(marked() == (1u << o));
    clear_marks();
    /* Outside a solve nothing is read or marked. */
    store.solving = false;
    store.evaluating_plus_one = r0 + 1;
    CHECK(function_param_flow_closure_read(&store, o) && store.closure_read_count == 2);
    function_param_flow_closure_changed(&store, r1);
    CHECK(marked() == 0);

    /* Every fact a reader can see is a change; bookkeeping is not. */
    bool outputs[2] = {false, false};
    FunctionParamFlowSummaryEntry base = {
        .function_decl = reader,
        .output_dependencies = outputs,
        .output_count = 2,
        .result_path_ready = true,
        .result_path = {.present = true, .count = 1, .fields = {101}},
    };
    FunctionParamFlowEntryFacts facts = function_param_flow_entry_facts(&base);
    CHECK(function_param_flow_entry_facts_equal(facts, function_param_flow_entry_facts(&base)));
    CHANGED(c.mask = SLOT_PARAM_SUMMARY_READ);
    CHANGED(c.array_storage_unproved = true);
    CHANGED(c.array_elements_unproved = true);
    CHANGED(c.returned_backing_unproved = true);
    CHANGED(c.returned_backing_pending = true);
    CHANGED(c.analysis_unsupported = true);
    CHANGED(c.output_dependencies_ready = true);
    CHANGED(c.output_producer = FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR);
    CHANGED(copied[1] = true);
    CHANGED(c.output_dependencies = NULL);
    CHANGED(c.result_path_ready = false);
    CHANGED(c.result_path.present = false);
    CHANGED(c.result_path.count = 0);
    CHANGED(c.result_path.fields[0] = 102);
    FunctionParamFlowSummaryEntry kept = base;
    kept.state = FUNCTION_PARAM_FLOW_COMPUTING;
    kept.dirty = true;
    kept.readers_plus_one = 3;
    kept.output_current_epoch = 9;
    CHECK(function_param_flow_entry_facts_equal(facts, function_param_flow_entry_facts(&kept)));

    /* A publication preparation owns one demand epoch. Ending it invalidates
     * that epoch for later consumers, and illegal nesting fails the store
     * closed instead of silently sharing an unrelated generation. */
    SemanticContext *publication_ctx = semantic_context_create();
    CHECK(publication_ctx != NULL);
    if (publication_ctx != NULL) {
        publication_ctx->program_root = program;
        CHECK(function_param_flow_publication_demand_begin(
            publication_ctx, reader));
        FunctionParamFlowSummaryStore *publication_store =
            publication_ctx->function_param_flow_summaries;
        CHECK(publication_store != NULL &&
            publication_store->publication_demand_active &&
            publication_store->demand_epoch == 1);
        CHECK(function_param_flow_publication_demand_end(publication_ctx));
        CHECK(!publication_store->publication_demand_active &&
            publication_store->demand_epoch == 2 &&
            !publication_store->failed);
        CHECK(!function_param_flow_publication_demand_end(publication_ctx));
        semantic_context_destroy(publication_ctx);
    }

    SemanticContext *nested_ctx = semantic_context_create();
    CHECK(nested_ctx != NULL);
    if (nested_ctx != NULL) {
        nested_ctx->program_root = program;
        CHECK(function_param_flow_publication_demand_begin(nested_ctx, reader));
        CHECK(!function_param_flow_publication_demand_begin(nested_ctx, reader));
        CHECK(nested_ctx->function_param_flow_summaries != NULL &&
            nested_ctx->function_param_flow_summaries->failed);
        semantic_context_destroy(nested_ctx);
    }

    SemanticContext *overflow_ctx = semantic_context_create();
    CHECK(overflow_ctx != NULL);
    if (overflow_ctx != NULL) {
        overflow_ctx->program_root = program;
        FunctionParamFlowSummaryStore *overflow_store =
            function_param_flow_store_get(&(SlotFunctionLookup){
                overflow_ctx, program});
        CHECK(overflow_store != NULL);
        if (overflow_store != NULL) {
            overflow_store->demand_epoch = SIZE_MAX;
            CHECK(!function_param_flow_publication_demand_begin(
                overflow_ctx, reader));
            CHECK(overflow_store->failed &&
                !overflow_store->publication_demand_active);
        }
        semantic_context_destroy(overflow_ctx);
    }

    if (failures != 0) {
        fprintf(stderr, "flow closure worklist: %u of %u checks failed\n", failures, checks);
        return 1;
    }
    printf("[flow-closure-worklist] %u checks passed\n", checks);
    return 0;
}
