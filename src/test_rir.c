/*
 * Copyright (c) 2026 Pergyra Language Project
 * RIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/dir.h"
#include "compiler/hir.h"
#include "compiler/rir.h"

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("ok\n"); g_pass++; } \
        else      { printf("FAIL (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

static bool
lower_rir_from_source(const char *source, HIRProgram **hir_out, RIRProgram **rir_out)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *hir_error = NULL;
    char *rir_error = NULL;
    *hir_out = NULL;
    *rir_out = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        *hir_out = hir_lower_with_resource_and_param_flow_facts(
            sem->annotated_ast,
            sem->resource_flow_facts,
            sem->resource_flow_fact_count,
            sem->function_param_flow_facts,
            sem->function_param_flow_fact_count,
            &hir_error);
        *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
    }

    if ((*hir_out == NULL || *rir_out == NULL) && (hir_error != NULL || rir_error != NULL)) {
        if (hir_error != NULL)
            fprintf(stderr, "HIR lowering error: %s\n", hir_error);
        if (rir_error != NULL)
            fprintf(stderr, "RIR lowering error: %s\n", rir_error);
    }

    free(hir_error);
    free(rir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return *hir_out != NULL && *rir_out != NULL;
}

static bool
lower_dir_rir_from_source(const char *source,
                          DIRProgram **dir_out,
                          HIRProgram **hir_out,
                          RIRProgram **rir_out)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *dir_error = NULL;
    char *hir_error = NULL;
    char *rir_error = NULL;
    *dir_out = NULL;
    *hir_out = NULL;
    *rir_out = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        *dir_out = dir_lower(sem->annotated_ast, &dir_error);
        *hir_out = hir_lower(sem->annotated_ast, &hir_error);
        *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
    }

    if ((*dir_out == NULL || *hir_out == NULL || *rir_out == NULL)
        && (dir_error != NULL || hir_error != NULL || rir_error != NULL)) {
        if (dir_error != NULL)
            fprintf(stderr, "DIR lowering error: %s\n", dir_error);
        if (hir_error != NULL)
            fprintf(stderr, "HIR lowering error: %s\n", hir_error);
        if (rir_error != NULL)
            fprintf(stderr, "RIR lowering error: %s\n", rir_error);
    }

    free(dir_error);
    free(hir_error);
    free(rir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return *dir_out != NULL && *hir_out != NULL && *rir_out != NULL;
}

static bool
scope_has_op(const RIRScope *scope, RIROpKind kind)
{
    for (size_t i = 0; i < scope->op_count; i++) {
        if (scope->ops[i].kind == kind)
            return true;
    }
    return false;
}

static bool
scope_has_op_subject(const RIRScope *scope, RIROpKind kind, const char *subject)
{
    for (size_t i = 0; i < scope->op_count; i++) {
        if (scope->ops[i].kind == kind
            && scope->ops[i].subject != NULL
            && strcmp(scope->ops[i].subject, subject) == 0)
            return true;
    }
    return false;
}

static bool
scope_has_resource_fact(const RIRScope *scope, const char *name, RIRResourceKind kind)
{
    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == RIR_FACT_RESOURCE
            && fact->name != NULL
            && strcmp(fact->name, name) == 0
            && fact->resource_kind == kind) {
            return true;
        }
    }
    return false;
}

static bool
scope_has_fact_slot_anchor(const RIRScope *scope,
                           RIRFactKind kind,
                           const char *name,
                           const char *slot_anchor)
{
    if (scope == NULL)
        return false;
    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == kind
            && fact->name != NULL
            && strcmp(fact->name, name) == 0
            && fact->slot_anchor != NULL
            && strcmp(fact->slot_anchor, slot_anchor) == 0) {
            return true;
        }
    }
    return false;
}

static bool
scope_has_projection_fact_kind(const RIRScope *scope,
                               const char *name,
                               RIRResourceKind kind,
                               RIRResourceState state)
{
    if (scope == NULL)
        return false;
    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == RIR_FACT_PROJECTION
            && fact->name != NULL
            && strcmp(fact->name, name) == 0
            && fact->resource_kind == kind
            && fact->state == state) {
            return true;
        }
    }
    return false;
}

static bool
scope_has_op_slot_anchor(const RIRScope *scope, RIROpKind kind, const char *slot_anchor)
{
    if (scope == NULL)
        return false;
    for (size_t i = 0; i < scope->op_count; i++) {
        const RIROp *op = &scope->ops[i];
        if (op->kind == kind
            && op->slot_anchor != NULL
            && strcmp(op->slot_anchor, slot_anchor) == 0) {
            return true;
        }
    }
    return false;
}

static const RIRStateSummary *
scope_find_state_summary(const RIRScope *scope, const char *name)
{
    if (scope == NULL)
        return NULL;
    for (size_t i = 0; i < scope->state_summary_count; i++) {
        if (scope->state_summaries[i].name != NULL
            && strcmp(scope->state_summaries[i].name, name) == 0) {
            return &scope->state_summaries[i];
        }
    }
    return NULL;
}

static const RIRScope *
find_scope(const RIRProgram *rir, const char *name, RIRScopeKind kind)
{
    if (rir == NULL)
        return NULL;
    for (size_t i = 0; i < rir->scope_count; i++) {
        if (rir->scopes[i].kind == kind
            && rir->scopes[i].name != NULL
            && strcmp(rir->scopes[i].name, name) == 0) {
            return &rir->scopes[i];
        }
    }
    return NULL;
}

static bool
scope_has_flow_semantics(const RIRScope *scope, unsigned int flags)
{
    if (scope == NULL)
        return false;
    for (size_t i = 0; i < scope->flow_block_count; i++) {
        const RIRFlowBlock *block = &scope->flow_blocks[i];
        if ((block->entry_semantics & flags) == flags
            || (block->exit_semantics & flags) == flags) {
            return true;
        }
    }
    return false;
}

static bool
scope_has_conservative_semantics(const RIRScope *scope, unsigned int flags)
{
    return scope != NULL && (scope->conservative_semantics & flags) == flags;
}

static void
test_rir_rejects_missing_hir_resource_identity(void)
{
    static const char *source =
        "func Main() -> Void {"
        "  let s: Slot<Int> = ClaimSlot();"
        "  Release(s);"
        "}";
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    char *error = NULL;
    bool lowered = lower_rir_from_source(source, &hir, &rir);

    TEST("RIR validation rejects a verified scope with missing HIR identity");
    if (!lowered || rir == NULL || rir->scope_count == 0
        || rir->scopes[0].fact_count == 0) {
        EXPECT(false);
        hir_destroy(hir);
        rir_destroy(rir);
        return;
    }

    rir->scopes[0].resource_identity_verified = true;
    rir->scopes[0].facts[0].has_flow_identity = false;
    if (rir->scopes[0].state_summary_count > 0)
        rir->scopes[0].state_summaries[0].has_flow_identity = false;
    EXPECT(!rir_validate(rir, &error)
           && error != NULL
           && strstr(error, "missing HIR stable identity") != NULL);
    free(error);
    hir_destroy(hir);
    rir_destroy(rir);
}

static void
test_rir_carries_function_param_flow_summary(void)
{
    static const char *source =
        "subject Vec2 { let x: Int; let y: Int; }\n"
        "func Recur(ref slot: Slot<Vec2>) -> Void {\n"
        "  Recur(slot);\n"
        "  Write(slot, Vec2(1, 2));\n"
        "}\n"
        "func Main() -> Void {\n"
        "  let slot: Slot<Vec2> = Vec2(0, 0);\n"
        "  Recur(slot);\n"
        "  Release(slot);\n"
        "}\n";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    char *error = NULL;
    bool carried = false;

    if (!parser_has_error(parser) && sem != NULL && sem->success
        && sem->function_param_flow_fact_count > 0) {
        hir = hir_lower_with_resource_and_param_flow_facts(
            sem->annotated_ast,
            sem->resource_flow_facts,
            sem->resource_flow_fact_count,
            sem->function_param_flow_facts,
            sem->function_param_flow_fact_count,
            &error);
        if (hir != NULL) {
            rir = rir_lower(sem->annotated_ast, &error);
            if (rir != NULL
                && rir_enrich_with_hir_flow(rir, hir, &error)
                && rir_validate(rir, &error)) {
                for (size_t i = 0; i < rir->scope_count; i++) {
                    if (rir->scopes[i].source_syntax_id
                            == sem->function_param_flow_facts[0].function_syntax_id
                        && rir_scope_function_param_flow_summary_count(
                            &rir->scopes[i]) > 0) {
                        carried = true;
                        break;
                    }
                }
            }
        }
    }

    TEST("RIR carries HIR function parameter flow summaries by stable identity");
    EXPECT(carried);
    free(error);
    hir_destroy(hir);
    rir_destroy(rir);
    semantic_result_destroy(sem);
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
}

static void
test_rir_rejects_unknown_resource_flow_identity(void)
{
    static const char *source =
        "func Main() -> Void {"
        "  let a: QubitSlot = ClaimQubit();"
        "  let b: QubitSlot = ClaimQubit();"
        "  Measure(a); Measure(b);"
        "  ReleaseQubit(a); ReleaseQubit(b);"
        "}";
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    char *error = NULL;
    bool lowered = lower_rir_from_source(source, &hir, &rir);

    TEST("RIR validation rejects a ResourceFlow fact with unknown stable identity");
    if (!lowered || rir == NULL || rir->scope_count == 0
        || rir->scopes[0].resource_flow_symbol_count == 0
        || rir->scopes[0].fact_count == 0) {
        EXPECT(false);
        hir_destroy(hir);
        rir_destroy(rir);
        return;
    }
    rir->scopes[0].facts[0].stable_index =
        rir->scopes[0].resource_flow_symbols[0].stable_index + 1000;
    EXPECT(!rir_validate(rir, &error)
           && error != NULL
           && strstr(error, "unknown ResourceFlow identity") != NULL);
    free(error);
    hir_destroy(hir);
    rir_destroy(rir);
}

static void
test_rir_rejects_duplicate_resource_flow_identity(void)
{
    static const char *source =
        "func Main() -> Void {"
        "  let a: QubitSlot = ClaimQubit();"
        "  let b: QubitSlot = ClaimQubit();"
        "  Measure(a); Measure(b);"
        "  ReleaseQubit(a); ReleaseQubit(b);"
        "}";
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    char *error = NULL;
    bool lowered = lower_rir_from_source(source, &hir, &rir);

    TEST("RIR validation rejects duplicate ResourceFlow stable identity");
    if (!lowered || rir == NULL || rir->scope_count == 0
        || rir->scopes[0].resource_flow_symbol_count < 2) {
        EXPECT(false);
        hir_destroy(hir);
        rir_destroy(rir);
        return;
    }
    rir->scopes[0].resource_flow_symbols[1].stable_index =
        rir->scopes[0].resource_flow_symbols[0].stable_index;
    EXPECT(!rir_validate(rir, &error)
           && error != NULL
           && strstr(error, "ResourceFlow symbols share identity") != NULL);
    free(error);
    hir_destroy(hir);
    rir_destroy(rir);
}

static void
test_rir_rejects_missing_dir_owner_identity(void)
{
    static const char *source =
        "subject Buyer { let total: Int; action Pay(self) -> Void { return; } }\n"
        "ability Payable { func Pay() -> Void; }\n"
        "role BuyerPay for Buyer {\n"
        "    impl ability Payable { func Pay() -> Void { return; } }\n"
        "}\n"
        "object ReceiptView { let total: Int; }\n"
        "tobject ReceiptExport { let total: Int; }\n"
        "zone PaymentZone {\n"
        "    subject slot buyer: Buyer\n"
        "    object slot preview: ReceiptView\n"
        "    tobject slot receipt_out: ReceiptExport\n"
        "    authority buyer requires Payable\n"
        "    refresh preview from buyer by buyer\n"
        "    publish receipt_out from preview by buyer\n"
        "}\n";
    DIRProgram *dir = NULL;
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    char *error = NULL;
    size_t slot_index = 0;
    bool found_slot = false;
    uint32_t slot_source_id = 0;
    bool lowered = lower_dir_rir_from_source(source, &dir, &hir, &rir);

    TEST("RIR/DIR validation rejects a slot with missing owner identity");
    if (lowered && dir != NULL && hir != NULL && rir != NULL
        && dir_validate(dir, &error)
        && rir_validate(rir, &error)) {
        for (size_t i = 0; i < dir->node_count; i++) {
            const DIRNode *node = &dir->nodes[i];
            if (node->kind == DIR_NODE_ZONE_SLOT
                || node->kind == DIR_NODE_PROJECTION_SLOT
                || node->kind == DIR_NODE_AUTHORITY_SLOT) {
                slot_index = i;
                found_slot = true;
                break;
            }
        }
    }
    free(error);
    error = NULL;
    if (!found_slot) {
        EXPECT(false);
        hir_destroy(hir);
        rir_destroy(rir);
        dir_destroy(dir);
        return;
    }

    EXPECT(dir->nodes[slot_index].source_syntax_id != 0
           && dir->nodes[slot_index].owner_source_syntax_id != 0);
    slot_source_id = dir->nodes[slot_index].source_syntax_id;
    dir->nodes[slot_index].owner_source_syntax_id = 0;
    EXPECT(!rir_validate_against_dir(rir, dir, &error)
           && error != NULL
           && strstr(error, "missing owner source identity") != NULL);
    free(error);
    error = NULL;
    dir->nodes[slot_index].owner_source_syntax_id = slot_source_id;
    EXPECT(!rir_validate_against_dir(rir, dir, &error)
           && error != NULL
           && strstr(error, "no RIR domain scope for owner source identity") != NULL);
    free(error);
    hir_destroy(hir);
    rir_destroy(rir);
    dir_destroy(dir);
}

#include "tests/rir/test_rir_lowering_1.cases.h"
#include "tests/rir/test_rir_lowering_2.cases.h"

int
main(void)
{
    printf("=== Pergyra RIR Lowering Test Suite ===\n");
    test_rir_lowering();
    test_rir_carries_function_param_flow_summary();
    test_rir_rejects_unknown_resource_flow_identity();
    test_rir_rejects_duplicate_resource_flow_identity();
    test_rir_rejects_missing_hir_resource_identity();
    test_rir_rejects_missing_dir_owner_identity();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
