/* Rows check memo keys, on forged store rows around one parsed routine.
 * A kept check answers again only while the routine's shape keeps its
 * version, and only a passed check on a settled shape is kept, in a solve
 * only without pending relations. The flow summary gate covers real stores, in both memo modes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/type_checker.h"
#include "semantic/function_param_flow/function_param_flow_summary_internal.h"

static unsigned checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { failures++; fprintf(stderr, "rows memo fault line %d: %s\n", __LINE__, #x); } } while (0)

static FunctionParamFlowSummaryStore store;
static FunctionParamFlowIndexClaims claims;
static FunctionParamFlowBackingFact frames[2];
static FunctionParamFlowBackingFact *slots[2];

static bool kept_after_record(bool passed)
{
    FunctionParamFlowRowsMemo reached = {0};
    function_param_flow_rows_memo_record(&store, 0, &reached, passed);
    return store.program_point_indexes[0].rows_memo.passed;
}

static bool current(void) { return function_param_flow_rows_memo_current(&store, 0); }

int main(void)
{
    Lexer *lexer = lexer_create("func Pair(a: Int, b: Int) -> Int { return Other(a); }\n"
        "func Plain(a: Int) -> Int { return a; }\n");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(ast_assign_stable_ids(program));
    ASTNode *decl = ast_program_statement(program, 0);
    CHECK(decl != NULL && decl->type == AST_FUNC_DECL && ast_func_param_count(decl) == 2);
    if (failures != 0) return 1;

    FunctionParamFlowProgramPointIndex *index = calloc(1, sizeof(*index));
    CHECK(index != NULL);
    if (index == NULL) return 1;
    store.program_point_indexes = index;
    store.program_point_index_count = store.program_point_index_capacity = 1;
    index->function_id = ast_node_stable_id(decl);
    index->function_decl = decl;
    index->param_count = 2;
    index->backing_frames = slots;
    index->backing_frame_count = 2;
    index->claims = &claims;
    frames[0] = frames[1] = (FunctionParamFlowBackingFact){.epoch = 1, .complete = true};
    slots[0] = &frames[0];
    size_t first = function_param_flow_add(&store, decl, 0);
    CHECK(first != SIZE_MAX);
    store.entries[first].state = FUNCTION_PARAM_FLOW_COMPLETE;

    /* Nothing is kept before a check passes. */
    CHECK(!current());
    CHECK(kept_after_record(true) && current());
    CHECK(current());

    /* A later demand adds an entry for the other formal. */
    size_t second = function_param_flow_add(&store, decl, 1);
    CHECK(second != SIZE_MAX && !current());
    CHECK(!kept_after_record(true));
    store.entries[second].state = FUNCTION_PARAM_FLOW_COMPLETE;
    CHECK(kept_after_record(true) && current());

    /* The new entry's frame appears, built and settled. */
    slots[1] = &frames[1];
    CHECK(!current());
    CHECK(kept_after_record(true) && current());

    /* A frame being built, or pending, is not settled. */
    frames[1].building = true;
    CHECK(!current() && !kept_after_record(true));
    frames[1].building = false;
    frames[1].pending = true;
    CHECK(!current() && !kept_after_record(true));
    frames[1].pending = false;
    frames[1].epoch = 0;
    CHECK(!kept_after_record(true));
    frames[1].epoch = 1;
    CHECK(kept_after_record(true) && current());

    /* Claims grow or fail. */
    claims.count = 1;
    CHECK(!current());
    CHECK(kept_after_record(true) && current());
    claims.failed = true;
    CHECK(!current());
    claims.failed = false;

    /* A declared result frame appears. */
    CHECK(kept_after_record(true) && current());
    FunctionParamFlowBackingFact result = {.epoch = 1, .complete = true};
    index->declared_result = &result;
    CHECK(!current());
    CHECK(kept_after_record(true) && current());

    /* Only a passed check is kept. A solve skips the check of pending
     * relation targets, so there a check is kept only when no frame has
     * pending relations. */
    CHECK(!kept_after_record(false) && !current());
    store.solving = true;
    CHECK(kept_after_record(true) && current());
    frames[0].pending_relation_count = frames[0].sealed_pending_relation_count = 1;
    CHECK(!kept_after_record(true) && !current());
    store.solving = false;
    CHECK(kept_after_record(true) && current());
    frames[0].pending_relation_count = frames[0].sealed_pending_relation_count = 0;
    store.failed = true;
    CHECK(!current());
    store.failed = false;

    /* A reuse repeats the checks of state outside the shape: here a no-store
     * graph and a read-only receiver that this context, without typed
     * facts, cannot prove. */
    store.ctx = semantic_context_create();
    store.work_budget = 4096;
    ASTNode *call = ast_return_value(ast_block_statement(ast_func_body(decl), 0));
    CHECK(store.ctx != NULL && call != NULL && call->type == AST_CALL);
    FunctionParamFlowRowsMemo reached = {0};
    CHECK(function_param_flow_rows_memo_note(&reached.no_store_routines, &reached.no_store_count, decl));
    function_param_flow_rows_memo_record(&store, 0, &reached, true);
    CHECK(store.program_point_indexes[0].rows_memo.passed && !current());
    reached = (FunctionParamFlowRowsMemo){0};
    CHECK(function_param_flow_rows_memo_note(&reached.receiver_calls, &reached.receiver_count, call));
    function_param_flow_rows_memo_record(&store, 0, &reached, true);
    CHECK(store.program_point_indexes[0].rows_memo.passed && !current());
    CHECK(kept_after_record(true) && current());

    /* The index claims check answers again until a row is added or
     * promoted, the locals change or the claims fail. A row changed behind
     * the owners' back shows which answers were reused. The routine has no
     * call, whose claim would need typed call facts this context lacks. */
    ASTNode *plain = ast_program_statement(program, 1);
    CHECK(plain != NULL && plain->type == AST_FUNC_DECL);
    if (plain == NULL) return 1;
    ASTNode *body = ast_func_body(plain);
    ASTNode *roots[1] = {ast_block_statement(body, 0)};
    uint32_t root_ids[1] = {ast_node_stable_id(roots[0])};
    FunctionParamFlowProgramPointIndex *claimed = calloc(1, sizeof(*claimed));
    CHECK(claimed != NULL);
    if (claimed == NULL) return 1;
    claimed->function_decl = plain;
    claimed->function_id = ast_node_stable_id(plain);
    claimed->dependency_body = body;
    claimed->dependency_body_id = ast_node_stable_id(body);
    claimed->dependency_roots = roots;
    claimed->dependency_root_ids = root_ids;
    claimed->dependency_root_count = 1;
    CHECK(function_param_flow_index_claims_snapshot(store.ctx, claimed, body));
    CHECK(function_param_flow_index_claims_current(store.ctx, claimed));
    FunctionParamFlowIndexClaims *rows = claimed->claims;
    uint32_t first_id = rows->rows[0].id;
    rows->rows[0].id = first_id + 1000;
    CHECK(function_param_flow_index_claims_current(store.ctx, claimed));
    ASTNode *added = ast_func_return_type(plain);
    CHECK(added != NULL && function_param_flow_claim_capture(
        store.ctx, rows, added, false));
    CHECK(!function_param_flow_index_claims_current(store.ctx, claimed));
    rows->rows[0].id = first_id;
    CHECK(function_param_flow_index_claims_current(store.ctx, claimed));
    rows->rows[0].id = first_id + 1000;
    CHECK(function_param_flow_claim_capture(
        store.ctx, rows, added, true));
    CHECK(!function_param_flow_index_claims_current(store.ctx, claimed));
    rows->rows[0].id = first_id;
    CHECK(function_param_flow_index_claims_current(store.ctx, claimed));
    ASTNode *locals[1] = {roots[0]};
    claimed->local_definitions = locals;
    claimed->local_count = 1;
    CHECK(!function_param_flow_index_claims_current(store.ctx, claimed));
    claimed->local_count = 0;
    CHECK(function_param_flow_index_claims_current(store.ctx, claimed));
    rows->failed = true;
    CHECK(!function_param_flow_index_claims_current(store.ctx, claimed));
    rows->failed = false;
    claimed->local_definitions = NULL;
    claimed->dependency_roots = NULL;
    claimed->dependency_root_ids = NULL;
    function_param_flow_program_point_index_destroy(claimed);
    free(claimed);
    semantic_context_destroy(store.ctx);
    store.ctx = NULL;

    index->declared_result = NULL;
    function_param_flow_rows_memo_destroy(index);
    free(index);
    free(store.entries);
    free(store.hash);
    fprintf(stderr, "flow rows check memo unit: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
