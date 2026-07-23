/*
 * Standalone unit test for the semantic region escape fact owner
 * (WO-REG-1 REG-1c, docs/197).
 *
 * The owner is verified against hand-built nodes with no driver link. Checks
 * the sound v1 rule: a string concat that is a DIRECT Print argument is
 * certified region-safe (with
 * its nested left-spine concats); anything else stays HEAP. Also checks
 * per-function scope-id allocation.
 *
 * Built + run by the `region-escape-unit-test-smoke` Make target.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "parser/ast.h"
#include "parser/ast_api.h"
#include "lexer/lexer.h"
#include "semantic/region_escape_fact.h"
#include "semantic/region_retention_summary.h"
#include "semantic/builtin_kind.h"

static int failures = 0;
static uint32_t next_test_stable_id = 1;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", (msg)); failures++; } \
} while (0)

static size_t collect_sites(const ASTNode *root, PgyRegionEscapeFact **sites)
{
    size_t count = 0;
    CHECK(semantic_region_escape_collect(
              root, NULL, NULL, sites, &count),
          "semantic region escape fact collection succeeds");
    return count;
}

static void test_retention_summary_owner(void)
{
    PgyRegionRetentionKind kind = PGY_REGION_RETENTION_UNKNOWN;

    CHECK(semantic_region_retention_summary_for_builtin(
              (uint32_t)BUILTIN_PRINT, 0, &kind),
          "Print argument retention summary is present");
    CHECK(kind == PGY_REGION_RETENTION_BORROWED_FOR_CALL,
          "Print argument retention summary is borrowed");
    CHECK(!semantic_region_retention_summary_for_builtin(
              (uint32_t)BUILTIN_PRINT, 1, &kind)
          && kind == PGY_REGION_RETENTION_UNKNOWN,
          "non-borrowed Print argument fails closed");
    CHECK(!semantic_region_retention_summary_for_builtin(
              (uint32_t)BUILTIN_WRITE, 0, &kind)
          && kind == PGY_REGION_RETENTION_UNKNOWN,
          "unknown builtin retention fails closed");
    CHECK(semantic_region_retention_summary_for_builtin(
              (uint32_t)BUILTIN_LOG, 1, &kind)
          && kind == PGY_REGION_RETENTION_BORROWED_FOR_CALL,
          "Log retains no argument beyond the synchronous call");
    CHECK(semantic_region_retention_summary_for_builtin(
              (uint32_t)BUILTIN_LOG_RAW, 0, &kind)
          && kind == PGY_REGION_RETENTION_BORROWED_FOR_CALL,
          "LogRaw argument retention summary is borrowed");
    CHECK(semantic_region_retention_summary_for_builtin(
              (uint32_t)BUILTIN_LOG_BANNER, 0, &kind)
          && kind == PGY_REGION_RETENTION_BORROWED_FOR_CALL,
          "LogBanner argument retention summary is borrowed");
    CHECK(semantic_region_retention_summary_for_builtin(
              (uint32_t)BUILTIN_LOG_BLOCK, 0, &kind)
          && kind == PGY_REGION_RETENTION_BORROWED_FOR_CALL,
          "LogBlock argument retention summary is borrowed");
}

static ASTNode mk_string(const char *v)
{
    ASTNode n; memset(&n, 0, sizeof(n));
    n.type = AST_STRING; n.stable_id = next_test_stable_id++;
    n.data.string.value = (char *)v; return n;
}
static ASTNode mk_ident(const char *name)
{
    ASTNode n; memset(&n, 0, sizeof(n));
    n.type = AST_IDENTIFIER; n.stable_id = next_test_stable_id++;
    n.data.identifier.name = (char *)name; return n;
}
static ASTNode mk_concat(ASTNode *l, ASTNode *r)
{
    ASTNode n; memset(&n, 0, sizeof(n));
    n.type = AST_BINARY; n.stable_id = next_test_stable_id++;
    n.data.binary.left = l; n.data.binary.right = r;
    n.data.binary.op.type = TOKEN_PLUS; return n;
}
static ASTNode mk_call(ASTNode *callee, ASTNode **args, size_t argc)
{
    ASTNode n; memset(&n, 0, sizeof(n));
    n.type = AST_CALL; n.stable_id = next_test_stable_id++;
    n.data.call.callee = callee;
    n.data.call.arguments = args; n.data.call.arg_count = argc; return n;
}

/* Print("a" + "b") -> the concat certified (1 site). */
static void test_print_concat_certified(void)
{
    ASTNode sa = mk_string("a"), sb = mk_string("b");
    ASTNode concat = mk_concat(&sa, &sb);
    ASTNode callee = mk_ident("Print");
    ASTNode *args[1] = { &concat };
    ASTNode call = mk_call(&callee, args, 1);
    CHECK(ast_call_set_semantic_callee_builtin_kind(
              &call, (uint32_t)BUILTIN_PRINT),
          "semantic Print builtin fact records");

    PgyRegionEscapeFact *sites = NULL;
    size_t n = collect_sites(&call, &sites);
    CHECK(n == 1, "Print(a+b) certifies 1 site");
    CHECK(n == 1 && sites[0].allocation_site_id == concat.stable_id,
          "certified site carries the concat stable id");
    semantic_region_escape_facts_free(sites);
}

static void test_log_family_concat_certified(void)
{
    const struct {
        BuiltinKind kind;
        const char *name;
    } cases[] = {
        { BUILTIN_LOG, "Log" },
        { BUILTIN_LOG_RAW, "LogRaw" },
        { BUILTIN_LOG_BANNER, "LogBanner" },
        { BUILTIN_LOG_BLOCK, "LogBlock" }
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        ASTNode sa = mk_string("a"), sb = mk_string("b");
        ASTNode concat = mk_concat(&sa, &sb);
        ASTNode callee = mk_ident(cases[i].name);
        ASTNode *args[1] = { &concat };
        ASTNode call = mk_call(&callee, args, 1);
        PgyRegionEscapeFact *sites = NULL;
        size_t n;

        CHECK(ast_call_set_semantic_callee_builtin_kind(
                  &call, (uint32_t)cases[i].kind),
              "semantic log-family builtin fact records");
        n = collect_sites(&call, &sites);
        CHECK(n == 1, "synchronous log-family concat certifies 1 site");
        semantic_region_escape_facts_free(sites);
    }
}

/* Print("a" + "b" + "c") -> outer ((a+b)+c) and inner (a+b) both certified. */
static void test_chained_concat_spine(void)
{
    ASTNode sa = mk_string("a"), sb = mk_string("b"), sc = mk_string("c");
    ASTNode inner = mk_concat(&sa, &sb);
    ASTNode outer = mk_concat(&inner, &sc);
    ASTNode callee = mk_ident("Print");
    ASTNode *args[1] = { &outer };
    ASTNode call = mk_call(&callee, args, 1);
    CHECK(ast_call_set_semantic_callee_builtin_kind(
              &call, (uint32_t)BUILTIN_PRINT),
          "semantic Print builtin fact records for chain");

    PgyRegionEscapeFact *sites = NULL;
    size_t n = collect_sites(&call, &sites);
    CHECK(n == 2, "Print(a+b+c) certifies 2 spine sites");
    semantic_region_escape_facts_free(sites);
}

/* A source-shaped Print call without its semantic builtin fact is not
 * certifiable. The region producer must not recover authority from spelling. */
static void test_missing_builtin_fact_not_certified(void)
{
    ASTNode sa = mk_string("a"), sb = mk_string("b");
    ASTNode concat = mk_concat(&sa, &sb);
    ASTNode callee = mk_ident("Print");
    ASTNode *args[1] = { &concat };
    ASTNode call = mk_call(&callee, args, 1);

    PgyRegionEscapeFact *sites = NULL;
    size_t n = collect_sites(&call, &sites);
    CHECK(n == 0, "missing semantic Print fact stays HEAP");
    semantic_region_escape_facts_free(sites);
}

/* Store("a" + "b") -> nothing certified (only semantic Print is borrow-safe). */
static void test_non_print_not_certified(void)
{
    ASTNode sa = mk_string("a"), sb = mk_string("b");
    ASTNode concat = mk_concat(&sa, &sb);
    ASTNode callee = mk_ident("Store");
    ASTNode *args[1] = { &concat };
    ASTNode call = mk_call(&callee, args, 1);

    PgyRegionEscapeFact *sites = NULL;
    size_t n = collect_sites(&call, &sites);
    CHECK(n == 0, "Store(a+b) certifies nothing");
    semantic_region_escape_facts_free(sites);
}

/* A bare concat with no enclosing Print -> HEAP (not certified). */
static void test_bare_concat_not_certified(void)
{
    ASTNode sa = mk_string("a"), sb = mk_string("b");
    ASTNode concat = mk_concat(&sa, &sb);
    PgyRegionEscapeFact *sites = NULL;
    size_t n = collect_sites(&concat, &sites);
    CHECK(n == 0, "bare concat certifies nothing");
    semantic_region_escape_facts_free(sites);
}

/* Two functions, each with a Print concat -> distinct scope ids. */
static void test_per_function_scope(void)
{
    ASTNode sa = mk_string("a"), sb = mk_string("b");
    ASTNode c1 = mk_concat(&sa, &sb);
    ASTNode callee1 = mk_ident("Print");
    ASTNode *a1[1] = { &c1 };
    ASTNode call1 = mk_call(&callee1, a1, 1);
    CHECK(ast_call_set_semantic_callee_builtin_kind(
              &call1, (uint32_t)BUILTIN_PRINT),
          "semantic Print builtin fact records for first function");
    ASTNode *b1stmts[1] = { &call1 };
    ASTNode blk1; memset(&blk1, 0, sizeof(blk1));
    blk1.type = AST_BLOCK; blk1.stable_id = next_test_stable_id++;
    blk1.data.block.statements = b1stmts; blk1.data.block.count = 1;
    ASTNode fn1; memset(&fn1, 0, sizeof(fn1));
    fn1.type = AST_FUNC_DECL; fn1.stable_id = next_test_stable_id++;
    fn1.data.func_decl.body = &blk1;

    ASTNode sc = mk_string("c"), sd = mk_string("d");
    ASTNode c2 = mk_concat(&sc, &sd);
    ASTNode callee2 = mk_ident("Print");
    ASTNode *a2[1] = { &c2 };
    ASTNode call2 = mk_call(&callee2, a2, 1);
    CHECK(ast_call_set_semantic_callee_builtin_kind(
              &call2, (uint32_t)BUILTIN_PRINT),
          "semantic Print builtin fact records for second function");
    ASTNode *b2stmts[1] = { &call2 };
    ASTNode blk2; memset(&blk2, 0, sizeof(blk2));
    blk2.type = AST_BLOCK; blk2.stable_id = next_test_stable_id++;
    blk2.data.block.statements = b2stmts; blk2.data.block.count = 1;
    ASTNode fn2; memset(&fn2, 0, sizeof(fn2));
    fn2.type = AST_FUNC_DECL; fn2.stable_id = next_test_stable_id++;
    fn2.data.func_decl.body = &blk2;

    ASTNode *progstmts[2] = { &fn1, &fn2 };
    ASTNode prog; memset(&prog, 0, sizeof(prog));
    prog.type = AST_PROGRAM; prog.stable_id = next_test_stable_id++;
    prog.data.program.statements = progstmts; prog.data.program.count = 2;

    PgyRegionEscapeFact *sites = NULL;
    size_t n = collect_sites(&prog, &sites);
    CHECK(n == 2, "two functions certify two sites");
    if (n == 2) {
        uint32_t s0 = sites[0].scope_id, s1 = sites[1].scope_id;
        CHECK(s0 != s1, "distinct functions get distinct scope ids");
        CHECK(s0 != 0 && s1 != 0, "function scopes are nonzero");
    }
    semantic_region_escape_facts_free(sites);
}

int main(void)
{
    test_retention_summary_owner();
    test_print_concat_certified();
    test_log_family_concat_certified();
    test_chained_concat_spine();
    test_missing_builtin_fact_not_certified();
    test_non_print_not_certified();
    test_bare_concat_not_certified();
    test_per_function_scope();
    if (failures) {
        fprintf(stderr, "%d region-escape checks failed\n", failures);
        return 1;
    }
    printf("region-escape-logic ok\n");
    return 0;
}
