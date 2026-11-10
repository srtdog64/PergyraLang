/* Recoverable allocation faults must neither publish a partial landing nor
 * take the source value away from the call/tree that still owns it. The
 * constructors retain their established fatal AST-node allocation boundary. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser/ast.h"
#include "parser/ast_api.h"
#include "semantic/type_checker.h"
#include "semantic/formal_string_store_landing_owner.h"

enum Allocation { MALLOC_FAULT, CALLOC_FAULT, REALLOC_FAULT };
static enum Allocation selected_allocation;
static size_t allocations_before_fault;
static bool fault_armed, fault_hit;
static unsigned checks, failures;

static bool fail_allocation(enum Allocation allocation)
{
    if (!fault_armed || allocation != selected_allocation) return false;
    if (allocations_before_fault != 0) {
        allocations_before_fault--;
        return false;
    }
    fault_armed = false;
    fault_hit = true;
    return true;
}

void *pgy_landing_test_malloc(size_t size)
{
    return fail_allocation(MALLOC_FAULT) ? NULL : malloc(size);
}

void *pgy_landing_test_calloc(size_t count, size_t size)
{
    return fail_allocation(CALLOC_FAULT) ? NULL : calloc(count, size);
}

void *pgy_landing_test_realloc(void *pointer, size_t size)
{
    return fail_allocation(REALLOC_FAULT) ? NULL : realloc(pointer, size);
}

#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    fprintf(stderr, "landing allocation fault line %d: %s\n", __LINE__, #condition); \
} } while (0)

static void allocation_faults(bool formal_store)
{
    for (enum Allocation allocation = MALLOC_FAULT;
         allocation <= REALLOC_FAULT; allocation++) {
        bool reached_success = false;
        for (size_t at = 0; at < 16; at++) {
            ASTNode *tree = ast_create_program();
            ASTNode *source = ast_create_identifier("source");
            ASTNode *second = NULL;
            ASTNode *call = NULL;
            SemanticContext *ctx = NULL;
            source->origin_path = strdup("landing-allocation.pgy");
            CHECK(source->origin_path != NULL);
            if (formal_store) {
                call = ast_create_call(ast_create_identifier("MapSet"));
                ast_add_argument(call, ast_create_identifier("rows"));
                ast_add_argument(call, ast_create_string("\"key\""));
                ast_add_argument(call, source);
                ast_add_statement(tree, call);
                ctx = semantic_context_create();
                CHECK(ctx != NULL);
            } else {
                ast_add_statement(tree, source);
                second = ast_create_identifier("second");
                second->origin_path = strdup("landing-allocation.pgy");
                CHECK(second->origin_path != NULL);
                ast_add_statement(tree, second);
            }
            CHECK(ast_assign_stable_ids(tree));
            uint32_t identity = ast_node_stable_id(source);
            uint32_t identities[2] = {identity, ast_node_stable_id(second)};
            uint32_t selected = ast_node_stable_id(call);
            if (ctx != NULL) {
                ctx->program_root = tree;
                ctx->string_store_landing_plan = &selected;
                ctx->string_store_landing_plan_count = 1;
            }
            selected_allocation = allocation;
            allocations_before_fault = at;
            fault_armed = true;
            fault_hit = false;
            bool landed = formal_store
                ? semantic_formal_string_store_landing(call, 0, 2, ctx)
                : ast_wrap_nodes_in_builtin_call(tree, identities, 2, "Clone");
            fault_armed = false;
            if (fault_hit) {
                CHECK(!landed);
                CHECK(source->type == AST_IDENTIFIER &&
                    ast_node_stable_id(source) == identity &&
                    strcmp(ast_identifier_name(source), "source") == 0);
                if (ctx != NULL) {
                    CHECK(ast_call_argument(call, 2) == source &&
                        ctx->synthetic_stable_id_next == 0);
                } else {
                    CHECK(second->type == AST_IDENTIFIER &&
                        ast_node_stable_id(second) == identities[1] &&
                        strcmp(ast_identifier_name(second), "second") == 0);
                }
                /* A refusal is restartable on the same tree, not only on a
                 * freshly parsed replacement with different identities. */
                landed = formal_store
                    ? semantic_formal_string_store_landing(call, 0, 2, ctx)
                    : ast_wrap_nodes_in_builtin_call(tree, identities, 2, "Clone");
                CHECK(landed);
            } else {
                CHECK(landed);
                reached_success = true;
            }
            if (ctx != NULL) semantic_context_destroy(ctx);
            ast_destroy(tree);
            if (reached_success) break;
        }
        CHECK(reached_success);
    }
}

int main(void)
{
    allocation_faults(false);
    allocation_faults(true);
    printf("OWNERSHIP LANDING ALLOCATION: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
