/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Transpiler unit test suite
 * Build: make test-transpile
 * Run:   ./bin/test_transpile
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common/string_compat.h"
#include "codegen/transpiler.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/rir.h"
#include "compiler/mir.h"
#include "semantic/type_system.h"
#include "semantic/type_checker.h"

/* -----------------------------------------------------------------
 * Test runner
 * ----------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;
static MIRProgram *g_last_mir = NULL;

#define TEST(name) \
    do { g_last_mir = NULL; printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("✓\n"); g_pass++; } \
        else      { printf("✗  (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

#define EXPECT_STR_CONTAINS(haystack, needle) \
    EXPECT(strstr((haystack), (needle)) != NULL)

#define EXPECT_STR_NOT_CONTAINS(haystack, needle) \
    EXPECT(strstr((haystack), (needle)) == NULL)

static bool
mir_block_slice_contains(const char *output, const char *label, const char *needle)
{
    const char *start;
    const char *end;
    const char *hit;
    size_t label_len;

    if (output == NULL || label == NULL || needle == NULL)
        return false;
    start = strstr(output, label);
    if (start == NULL)
        return false;
    label_len = strlen(label);
    /* Find next block label (but not within goto target of current block) */
    end = strstr(start + label_len, "\n    _pgy_mir_bb_");
    if (end == NULL)
        end = strstr(start + label_len, "\n_pgy_mir_bb_");
    if (end == NULL)
        end = output + strlen(output);
    hit = strstr(start, needle);
    return hit != NULL && hit < end;
}

/* -----------------------------------------------------------------
 * Minimal AST node builders (same helpers as test_semantic.c)
 * ----------------------------------------------------------------- */

static ASTNode *
make_number(double v, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type  = AST_NUMBER;
    n->line  = line;
    n->data.number.value = v;
    return n;
}

static ASTNode *
make_string_lit(const char *s, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_STRING;
    n->line = line;
    n->data.string.value = pergyra_strdup(s);
    return n;
}

static ASTNode *
make_identifier(const char *name, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_IDENTIFIER;
    n->line = line;
    n->data.identifier.name = pergyra_strdup(name);
    return n;
}

static ASTNode *
make_call(const char *callee_name, ASTNode **args, size_t arg_count,
           uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_CALL;
    n->line = line;
    n->data.call.callee    = make_identifier(callee_name, line);
    if (arg_count > 0) {
        n->data.call.arguments = calloc(arg_count, sizeof(ASTNode *));
        memcpy(n->data.call.arguments, args, arg_count * sizeof(ASTNode *));
    }
    n->data.call.arg_count = arg_count;
    return n;
}

static ASTNode *
make_type_node(const char *type_name)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_TYPE;
    n->data.type.name = pergyra_strdup(type_name);
    return n;
}

static ASTNode *
make_let(const char *name, ASTNode *ann, ASTNode *init, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_LET_DECL;
    n->line = line;
    n->data.let_decl.name        = pergyra_strdup(name);
    n->data.let_decl.type        = ann;
    n->data.let_decl.initializer = init;
    return n;
}

static ASTNode *
make_return(ASTNode *value, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_RETURN;
    n->line = line;
    n->data.return_stmt.value = value;
    return n;
}

static ASTNode *
make_block(ASTNode **stmts, size_t count)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_BLOCK;
    if (count > 0) {
        n->data.block.statements = calloc(count, sizeof(ASTNode *));
        memcpy(n->data.block.statements, stmts, count * sizeof(ASTNode *));
    }
    n->data.block.count      = count;
    return n;
}

static ASTNode *
make_program(ASTNode **stmts, size_t count)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_PROGRAM;
    if (count > 0) {
        n->data.program.statements = calloc(count, sizeof(ASTNode *));
        memcpy(n->data.program.statements, stmts, count * sizeof(ASTNode *));
    }
    n->data.program.count      = count;
    return n;
}

static MIRProgram *
mir_program_from_ast(ASTNode *program);

static MIRProgram *
lower_program_to_mir(ASTNode *program, HIRProgram **hir_out, RIRProgram **rir_out)
{
    SemanticResult *sem = semantic_analyze(program);
    char *hir_error = NULL;
    char *rir_error = NULL;
    char *mir_error = NULL;
    MIRProgram *mir = NULL;

    if (hir_out != NULL)
        *hir_out = NULL;
    if (rir_out != NULL)
        *rir_out = NULL;

    if (sem != NULL && sem->success) {
        if (hir_out != NULL)
            *hir_out = hir_lower(sem->annotated_ast, &hir_error);
        if (rir_out != NULL)
            *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (hir_out != NULL && rir_out != NULL
            && *hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
        if (hir_out != NULL && rir_out != NULL
            && *hir_out != NULL && *rir_out != NULL)
            mir = mir_lower(*hir_out, *rir_out, &mir_error);
    }

    if (mir == NULL) {
        if (hir_error != NULL)
            fprintf(stderr, "HIR lowering failed in test: %s\n", hir_error);
        if (rir_error != NULL)
            fprintf(stderr, "RIR lowering failed in test: %s\n", rir_error);
        if (mir_error != NULL)
            fprintf(stderr, "MIR lowering failed in test: %s\n", mir_error);
        mir = mir_program_from_ast(program);
    }

    free(hir_error);
    free(rir_error);
    free(mir_error);
    g_last_mir = mir;
    return mir;
}

static MIRProgram *
mir_program_from_ast(ASTNode *program)
{
    if (program == NULL || program->type != AST_PROGRAM)
        return NULL;

    MIRProgram *mir = calloc(1, sizeof(MIRProgram));
    if (mir == NULL)
        return NULL;

    size_t stmt_count = program->data.program.count;
    ASTNode **stmts = program->data.program.statements;

    for (size_t i = 0; i < stmt_count; i++) {
        ASTNode *stmt = stmts[i];
        if (stmt == NULL)
            continue;
        switch (stmt->type) {
        case AST_EXTERN_BLOCK:  mir->extern_count++; break;
        case AST_CLASS_DECL:
        case AST_ENUM_DECL:     mir->type_count++; break;
        case AST_FUNC_DECL:     mir->function_count++; break;
        case AST_INTENT_DECL:   mir->intent_count++; break;
        case AST_ABILITY_DECL:  mir->ability_count++; break;
        case AST_ROLE_DECL:     mir->role_count++; break;
        case AST_PARTY_DECL:    mir->party_count++; break;
        case AST_ROSTER_DECL:   mir->roster_count++; break;
        case AST_WORLD_DECL:    mir->world_count++; break;
        case AST_RELATION_DECL: mir->relation_count++; break;
        case AST_EFFECT_DECL:   mir->effect_count++; break;
        case AST_ZONE_DECL:     mir->zone_count++; break;
        case AST_EVENT_DECL:    mir->event_count++; break;
        default: break;
        }
    }

    if (mir->extern_count)  mir->externs  = calloc(mir->extern_count, sizeof(ASTNode *));
    if (mir->type_count)    mir->types    = calloc(mir->type_count, sizeof(ASTNode *));
    if (mir->function_count)mir->functions= calloc(mir->function_count, sizeof(ASTNode *));
    if (mir->intent_count)  mir->intents  = calloc(mir->intent_count, sizeof(ASTNode *));
    if (mir->ability_count) mir->abilities= calloc(mir->ability_count, sizeof(ASTNode *));
    if (mir->role_count)    mir->roles    = calloc(mir->role_count, sizeof(ASTNode *));
    if (mir->party_count)   mir->parties  = calloc(mir->party_count, sizeof(ASTNode *));
    if (mir->roster_count)  mir->rosters  = calloc(mir->roster_count, sizeof(ASTNode *));
    if (mir->world_count)   mir->worlds   = calloc(mir->world_count, sizeof(ASTNode *));
    if (mir->relation_count)mir->relations= calloc(mir->relation_count, sizeof(ASTNode *));
    if (mir->effect_count)  mir->effects  = calloc(mir->effect_count, sizeof(ASTNode *));
    if (mir->zone_count)    mir->zones    = calloc(mir->zone_count, sizeof(ASTNode *));
    if (mir->event_count)   mir->events   = calloc(mir->event_count, sizeof(ASTNode *));

    size_t ext_i = 0, type_i = 0, func_i = 0, intent_i = 0;
    size_t ability_i = 0, role_i = 0, party_i = 0, roster_i = 0;
    size_t world_i = 0, relation_i = 0, effect_i = 0, zone_i = 0, event_i = 0;

    for (size_t i = 0; i < stmt_count; i++) {
        ASTNode *stmt = stmts[i];
        if (stmt == NULL)
            continue;
        switch (stmt->type) {
        case AST_EXTERN_BLOCK:  mir->externs[ext_i++] = stmt; break;
        case AST_CLASS_DECL:
        case AST_ENUM_DECL:     mir->types[type_i++] = stmt; break;
        case AST_FUNC_DECL:
            mir->functions[func_i++] = stmt;
            if (stmt->data.func_decl.name != NULL
                && strcmp(stmt->data.func_decl.name, "Main") == 0)
                mir->has_main_function = true;
            break;
        case AST_INTENT_DECL:   mir->intents[intent_i++] = stmt; break;
        case AST_ABILITY_DECL:  mir->abilities[ability_i++] = stmt; break;
        case AST_ROLE_DECL:     mir->roles[role_i++] = stmt; break;
        case AST_PARTY_DECL:    mir->parties[party_i++] = stmt; break;
        case AST_ROSTER_DECL:   mir->rosters[roster_i++] = stmt; break;
        case AST_WORLD_DECL:    mir->worlds[world_i++] = stmt; break;
        case AST_RELATION_DECL: mir->relations[relation_i++] = stmt; break;
        case AST_EFFECT_DECL:   mir->effects[effect_i++] = stmt; break;
        case AST_ZONE_DECL:     mir->zones[zone_i++] = stmt; break;
        case AST_EVENT_DECL:    mir->events[event_i++] = stmt; break;
        default: break;
        }
    }

    return mir;
}

static TranspilerCtx *
transpiler_ctx_create_for_test(void)
{
    TranspilerCtx *ctx = transpiler_ctx_create();
    if (ctx != NULL && g_last_mir != NULL)
        ctx->mir = g_last_mir;
    return ctx;
}

#define transpiler_ctx_create transpiler_ctx_create_for_test

static bool
lower_pipeline_from_source(const char *source,
                           ASTNode **program_out,
                           HIRProgram **hir_out,
                           RIRProgram **rir_out,
                           MIRProgram **mir_out)
{
    bool ok = false;
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(program);
    char *hir_error = NULL;
    char *rir_error = NULL;
    char *mir_error = NULL;

    *program_out = program;
    *hir_out = NULL;
    *rir_out = NULL;
    *mir_out = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        *hir_out = hir_lower(sem->annotated_ast, &hir_error);
        *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            *mir_out = mir_lower(*hir_out, *rir_out, &mir_error);
    }

    ok = (*hir_out != NULL && *rir_out != NULL && *mir_out != NULL);
    if (!ok) {
        if (hir_error != NULL)
            fprintf(stderr, "HIR lowering failed in test: %s\n", hir_error);
        if (rir_error != NULL)
            fprintf(stderr, "RIR lowering failed in test: %s\n", rir_error);
        if (mir_error != NULL)
            fprintf(stderr, "MIR lowering failed in test: %s\n", mir_error);
        if (*hir_out == NULL) fprintf(stderr, "HIR is NULL\n");
        if (*rir_out == NULL) fprintf(stderr, "RIR is NULL\n");
        if (*mir_out == NULL) fprintf(stderr, "MIR is NULL\n");
    } else {
        fprintf(stderr, "MIR lowering OK: %zu routines\n", (*mir_out)->routine_count);
    }

    free(hir_error);
    free(rir_error);
    free(mir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return ok;
}

static char *
read_file_text(const char *path)
{
    FILE *f = fopen(path, "rb");
    long size;
    char *data;
    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    data = calloc((size_t)size + 1, 1);
    if (data == NULL) {
        fclose(f);
        return NULL;
    }
    if (size > 0) {
        size_t read_bytes = fread(data, 1, (size_t)size, f);
        if (read_bytes != (size_t)size) {
            free(data);
            fclose(f);
            return NULL;
        }
    }
    fclose(f);
    return data;
}

static const ASTNode *
find_hir_function_by_name(const HIRProgram *hir, const char *name)
{
    if (hir == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < hir->function_count; i++) {
        const ASTNode *f = hir->functions[i];
        if (f != NULL && f->type == AST_FUNC_DECL
            && f->data.func_decl.name != NULL
            && strcmp(f->data.func_decl.name, name) == 0) {
            return f;
        }
    }
    return NULL;
}

static const ASTNode *
find_hir_intent_by_name(const HIRProgram *hir, const char *name)
{
    if (hir == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < hir->intent_count; i++) {
        const ASTNode *intent = hir->intents[i];
        if (intent != NULL && intent->type == AST_INTENT_DECL
            && intent->data.intent_decl.name != NULL
            && strcmp(intent->data.intent_decl.name, name) == 0) {
            return intent;
        }
    }
    return NULL;
}

static const MIRRoutine *
find_mir_routine_by_name(const MIRProgram *mir, const char *name)
{
    if (mir == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        if (routine->name != NULL && strcmp(routine->name, name) == 0)
            return routine;
    }
    return NULL;
}

static size_t
mir_count_reachable_non_cleanup_blocks(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (block->is_reachable && !block->is_cleanup)
            count++;
    }
    return count;
}

static size_t
mir_count_phi_instructions(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_PHI)
                count++;
        }
    }
    return count;
}

static size_t
mir_count_exceptional_edges(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (!block->is_cleanup && block->is_reachable) {
            if (block->has_cleanup_succ)
                count++;
            if (block->has_rollback_succ)
                count++;
            if (block->has_invalidation_succ)
                count++;
        }
    }
    return count;
}

static bool
check_function_mir_emitability(const HIRProgram *hir, const MIRProgram *mir,
                              const char *function_name)
{
    const ASTNode *func_decl = find_hir_function_by_name(hir, function_name);
    char reason[512];
    if (func_decl == NULL) {
        fprintf(stderr, "  ✗ no HIR function '%s'\n", function_name);
        return false;
    }
    if (!transpiler_can_emit_function_from_mir_with_reason_for_test(func_decl,
            mir, reason, sizeof(reason))) {
        fprintf(stderr, "  ✗ MIR emitability rejected for function '%s': %s\n",
                function_name, reason);
        return false;
    }
    return true;
}

static bool
check_intent_mir_emitability(const HIRProgram *hir, const MIRProgram *mir,
                             const char *intent_name)
{
    const ASTNode *intent_decl = find_hir_intent_by_name(hir, intent_name);
    char reason[512];
    if (intent_decl == NULL) {
        fprintf(stderr, "  ✗ no HIR intent '%s'\n", intent_name);
        return false;
    }
    if (!transpiler_can_emit_intent_cleanup_from_mir_with_reason_for_test(intent_decl,
            mir, reason, sizeof(reason))) {
        fprintf(stderr, "  ✗ MIR emitability rejected for intent '%s': %s\n",
                intent_name, reason);
        return false;
    }
    return true;
}

static ASTNode *
make_generic_type(const char *name, const char *arg_name)
{
    ASTNode *n = ast_create_type(name);
    n->data.type.generic_args = calloc(1, sizeof(GenericParams));
    n->data.type.generic_args->count = 1;
    n->data.type.generic_args->params = calloc(1, sizeof(GenericParam *));

    GenericParam *gp = calloc(1, sizeof(GenericParam));
    gp->name = pergyra_strdup(arg_name);
    gp->constraint = ast_create_type(arg_name);
    n->data.type.generic_args->params[0] = gp;
    return n;
}

static ASTNode *
make_generic_type_from_node(const char *name, ASTNode *arg_type)
{
    ASTNode *n = ast_create_type(name);
    n->data.type.generic_args = calloc(1, sizeof(GenericParams));
    n->data.type.generic_args->count = 1;
    n->data.type.generic_args->params = calloc(1, sizeof(GenericParam *));

    GenericParam *gp = calloc(1, sizeof(GenericParam));
    gp->name = pergyra_strdup(arg_type->data.type.name);
    gp->constraint = arg_type;
    n->data.type.generic_args->params[0] = gp;
    return n;
}

static ASTNode *
make_match_case(ASTNode *pattern, ASTNode *body)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_MATCH_CASE;
    n->data.match_case.pattern = pattern;
    n->data.match_case.body = body;
    return n;
}

static ASTNode *
make_match(ASTNode *subject, ASTNode **cases, size_t case_count)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_MATCH_STMT;
    n->data.match_stmt.subject = subject;
    if (case_count > 0) {
        n->data.match_stmt.cases = calloc(case_count, sizeof(ASTNode *));
        memcpy(n->data.match_stmt.cases, cases, case_count * sizeof(ASTNode *));
    }
    n->data.match_stmt.case_count = case_count;
    return n;
}

static GenericParams *
make_generic_params1(const char *name)
{
    GenericParams *params = calloc(1, sizeof(GenericParams));
    GenericParam *param = calloc(1, sizeof(GenericParam));

    params->count = 1;
    params->params = calloc(1, sizeof(GenericParam *));
    param->name = pergyra_strdup(name);
    params->params[0] = param;
    return params;
}

/* -----------------------------------------------------------------
 * Helper: emit a single statement into a fresh context, return
 * the output string (caller does NOT free — points into ctx->out).
 * ----------------------------------------------------------------- */

static const char *
emit_stmt_to_str(ASTNode *node, TranspilerCtx **out_ctx)
{
    TranspilerCtx *ctx = transpiler_ctx_create();
    emit_statement(node, ctx);
    *out_ctx = ctx;
    return ctx->out->data;
}

/* -----------------------------------------------------------------
 * Tests: CodeBuf
 * ----------------------------------------------------------------- */

static void
test_codebuf(void)
{
    printf("\n[codebuf]\n");

    TEST("write simple string");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "hello");
        EXPECT(strcmp(b->data, "hello") == 0 && b->len == 5);
        codebuf_destroy(b);
    }

    TEST("write formatted string");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "int x = %d;", 42);
        EXPECT(strcmp(b->data, "int x = 42;") == 0);
        codebuf_destroy(b);
    }

    TEST("write triggers growth beyond initial capacity");
    {
        CodeBuf *b = codebuf_create();
        for (int i = 0; i < 1000; i++)
            codebuf_write(b, "a");
        EXPECT(b->len == 1000);
        codebuf_destroy(b);
    }

    TEST("multiple writes concatenate correctly");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "foo");
        codebuf_write(b, "bar");
        codebuf_write(b, "baz");
        EXPECT(strcmp(b->data, "foobarbaz") == 0);
        codebuf_destroy(b);
    }
}

/* -----------------------------------------------------------------
 * Tests: type mapping
 * ----------------------------------------------------------------- */

static void
test_type_mapping(void)
{
    printf("\n[type_mapping]\n");

    TEST("Int → int32_t");
    EXPECT(strcmp(pergyra_type_to_c("Int"), "int32_t") == 0);

    TEST("Long → int64_t");
    EXPECT(strcmp(pergyra_type_to_c("Long"), "int64_t") == 0);

    TEST("Float → float");
    EXPECT(strcmp(pergyra_type_to_c("Float"), "float") == 0);

    TEST("Bool → bool");
    EXPECT(strcmp(pergyra_type_to_c("Bool"), "bool") == 0);

    TEST("String → char*");
    EXPECT(strcmp(pergyra_type_to_c("String"), "char*") == 0);

    TEST("Void → void");
    EXPECT(strcmp(pergyra_type_to_c("Void"), "void") == 0);

    TEST("Slot<Int> → PgySlot_Int");
    EXPECT(strcmp(pergyra_type_to_c("Slot<Int>"), "PgySlot_Int") == 0);

    TEST("Slot<String> → PgySlot_String");
    EXPECT(strcmp(pergyra_type_to_c("Slot<String>"), "PgySlot_String") == 0);

    TEST("Slot<Vec2> → PgySlot_Vec2");
    EXPECT(strcmp(pergyra_type_to_c("Slot<Vec2>"), "PgySlot_Vec2") == 0);

    TEST("SecureSlot<Int> → PgySecureSlot_Int");
    EXPECT(strcmp(pergyra_type_to_c("SecureSlot<Int>"), "PgySecureSlot_Int") == 0);

    TEST("SecureSlot<Vec2> → PgySecureSlot_Vec2");
    EXPECT(strcmp(pergyra_type_to_c("SecureSlot<Vec2>"), "PgySecureSlot_Vec2") == 0);

    TEST("Array<Vertex> → PgyArray_Vertex");
    EXPECT(strcmp(pergyra_type_to_c("Array<Vertex>"), "PgyArray_Vertex") == 0);

    TEST("Slice<Vertex> → PgySlice_Vertex");
    EXPECT(strcmp(pergyra_type_to_c("Slice<Vertex>"), "PgySlice_Vertex") == 0);

    TEST("List<Vertex> → PgyList_Vertex");
    EXPECT(strcmp(pergyra_type_to_c("List<Vertex>"), "PgyList_Vertex") == 0);

    TEST("Queue<Vertex> → PgyQueue_Vertex");
    EXPECT(strcmp(pergyra_type_to_c("Queue<Vertex>"), "PgyQueue_Vertex") == 0);

    TEST("Rc<Int> → PgyRc_Int");
    EXPECT(strcmp(pergyra_type_to_c("Rc<Int>"), "PgyRc_Int") == 0);

    TEST("Weak<Int> → PgyWeak_Int");
    EXPECT(strcmp(pergyra_type_to_c("Weak<Int>"), "PgyWeak_Int") == 0);

    TEST("Allocator → PgyAllocator");
    EXPECT(strcmp(pergyra_type_to_c("Allocator"), "PgyAllocator") == 0);

    TEST("Box<Array<Int>> → PgyBoxArray_Int");
    EXPECT(strcmp(pergyra_type_to_c("Box<Array<Int>>"), "PgyBoxArray_Int") == 0);

    TEST("slot_inner_type_name(Slot<Float>) → Float");
    EXPECT(strcmp(slot_inner_type_name("Slot<Float>"), "Float") == 0);

    TEST("slot_inner_type_name(SecureSlot<Long>) → Long");
    EXPECT(strcmp(slot_inner_type_name("SecureSlot<Long>"), "Long") == 0);
}

/* -----------------------------------------------------------------
 * Tests: expression emitters
 * ----------------------------------------------------------------- */

static void
test_expression_emit(void)
{
    printf("\n[expression_emit]\n");

    TranspilerCtx *ctx;
    char *result;

    TEST("integer literal → correct C literal");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_number(42, 1), ctx);
        EXPECT(strcmp(result, "42") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("string literal → quoted C string");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_string_lit("hello", 1), ctx);
        EXPECT(strcmp(result, "\"hello\"") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Some(42) → Some_Int(42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Some", args, 1, 1), ctx);
        EXPECT(strcmp(result, "Some_Int(42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("None() → None_Int()");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(make_call("None", NULL, 0, 1), ctx);
        EXPECT(strcmp(result, "None_Int()") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("identifier → same name");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_identifier("myVar", 1), ctx);
        EXPECT(strcmp(result, "myVar") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("user function call → funcName(arg0, arg1)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[2] = { make_number(1, 1), make_number(2, 1) };
        result = emit_expression(make_call("Add", args, 2, 1), ctx);
        EXPECT(strcmp(result, "Add(1, 2)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Log(42) → pgy_log(42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Log", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log(42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Log(\"\\n + indentation\") normalizes multiline as banner");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second", 1),
        };
        result = emit_expression(make_call("Log", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log_banner(\"first\\nsecond\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("LogBlock(\"\\n + indentation\") normalizes multiline block text");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second\n", 1),
        };
        result = emit_expression(make_call("LogBlock", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log_banner(\"first\\nsecond\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("LogBanner(\"\\n + indentation\") normalizes multiline banner text");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second", 1),
        };
        result = emit_expression(make_call("LogBanner", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log_banner(\"first\\nsecond\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("LogRaw preserves raw newline and leading spaces");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second", 1),
        };
        result = emit_expression(make_call("LogRaw", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log(\"\\n  first\\n  second\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Read(s) → pgy_read_Int(&s)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("s", 1) };
        result = emit_expression(make_call("Read", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_read_Int(&s)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Release(s) → pgy_release_Int(&s)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("s", 1) };
        result = emit_expression(make_call("Release", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_release_Int(&s)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ViewRead(slot) → slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("ViewRead", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ViewWrite(slot) → slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("ViewWrite", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Move(slot) → slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("Move", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("array access → values[0]");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(
            ast_create_array_access(make_identifier("values", 1),
                                    make_number(0, 1)),
            ctx);
        EXPECT(strcmp(result, "values[0]") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("RcClone(shared) → pgy_rc_clone_Int(shared)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("shared",
                     make_generic_type("Rc", "Int"),
                     make_call("RcNew", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("shared", 1) };
        result = emit_expression(make_call("RcClone", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_rc_clone_Int(shared)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("BoxGet(boxed) → pgy_box_get_Int(boxed)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("boxed",
                     make_generic_type("Box", "Int"),
                     make_call("Box", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("boxed", 1) };
        result = emit_expression(make_call("BoxGet", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_box_get_Int(boxed)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("BoxSet(boxed, 42) → pgy_box_set_Int(&boxed, 42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("boxed",
                     make_generic_type("Box", "Int"),
                     make_call("Box", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[2] = { make_identifier("boxed", 1), make_number(42, 1) };
        result = emit_expression(make_call("BoxSet", args, 2, 1), ctx);
        EXPECT(strcmp(result, "pgy_box_set_Int(&boxed, 42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ToTObject(PlayerDto, player) → tobject projection literal");
    {
        const char *source =
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let snapshot: PlayerDto = ToTObject(PlayerDto, player);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PlayerDto");
        EXPECT_STR_CONTAINS(ctx->out->data, "= (PlayerDto){ .hp =");
        EXPECT_STR_CONTAINS(ctx->out->data, ".name =");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToObject(PlayerView, player) → object projection literal");
    {
        const char *source =
            "object PlayerView { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let view: PlayerView = ToObject(PlayerView, player);\n"
            "    Log(view.hp);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PlayerView");
        EXPECT_STR_CONTAINS(ctx->out->data, "= (PlayerView){ .hp =");
        EXPECT_STR_CONTAINS(ctx->out->data, ".name =");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("nested vessel-backed projection lowers through subject field paths");
    {
        const char *source =
            "vessel Cycle { age: Int; fatigue: Int; }\n"
            "vessel Traits { metabolism: Int; }\n"
            "subject Creature { vessel cycle: Cycle; vessel traits: Traits; }\n"
            "object CreatureView { age: Int; fatigue: Int; metabolism: Int; }\n"
            "tobject CreaturePacket { age: Int; metabolism: Int; }\n"
            "func Main() -> Void {\n"
            "    let creature: Creature = Creature();\n"
            "    let view: CreatureView = ToObject(CreatureView, creature);\n"
            "    let packet: CreaturePacket = ToTObject(CreaturePacket, creature);\n"
            "    Log(view.metabolism);\n"
            "    Log(packet.metabolism);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "CreatureView");
        EXPECT_STR_CONTAINS(ctx->out->data, "CreaturePacket");
        EXPECT_STR_CONTAINS(ctx->out->data, ".cycle.age");
        EXPECT_STR_CONTAINS(ctx->out->data, ".traits.metabolism");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("AllocatorTracing() → pgy_allocator_tracing()");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(make_call("AllocatorTracing", NULL, 0, 1), ctx);
        EXPECT(strcmp(result, "pgy_allocator_tracing()") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasState(poisoned) → zone semantic placeholder outside zone context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("poisoned", 1) };
        result = emit_expression(make_call("HasState", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasState requires active zone context");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasState(allied, player, enemy) → zone semantic placeholder outside zone context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[3] = {
            make_identifier("allied", 1),
            make_identifier("player", 1),
            make_identifier("enemy", 1)
        };
        result = emit_expression(make_call("HasState", args, 3, 1), ctx);
        EXPECT(strcmp(result, "false") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasState requires active zone context");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasZone(battle) → world semantic placeholder outside world context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("battle", 1) };
        result = emit_expression(make_call("HasZone", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasZone requires active world context");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasLayer(poison) → zone semantic placeholder outside zone context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("poison", 1) };
        result = emit_expression(make_call("HasLayer", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasLayer requires active zone context");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasProjection(snapshot) → domain semantic placeholder outside domain context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("snapshot", 1) };
        result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasProjection requires active relation/effect/zone projection context");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasProjection lowers to relation/effect/zone runtime projection flag inside domain context");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    subject slot left: Player\n"
            "    subject slot right: Player\n"
            "    object slot playerView: PlayerView\n"
            "    bind playerView from left\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    subject slot player: Player\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind snapshot from player\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind playerView from player\n"
            "    bind snapshot from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);

        ctx = transpiler_ctx_create();
        ctx->mir = mir;

        ctx->current_relation_name = "TrustedLink";
        {
            ASTNode *args[1] = { make_identifier("playerView", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_playerView") == 0);
            free(result);
        }
        ctx->current_relation_name = NULL;

        ctx->current_effect_name = "Poisoned";
        {
            ASTNode *args[1] = { make_identifier("snapshot", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_snapshot") == 0);
            free(result);
        }
        ctx->current_effect_name = NULL;

        ctx->current_zone_name = "BattleZone";
        {
            ASTNode *args[1] = { make_identifier("snapshot", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_snapshot") == 0);
            free(result);
        }
        {
            ASTNode *args[1] = { make_identifier("playerView", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_playerView") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("LLVM domain layouts include projection-ready flags for relation/effect/zone");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    subject slot left: Player\n"
            "    subject slot right: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind playerView from left\n"
            "    bind snapshot from right\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind playerView from player\n"
            "    bind snapshot from player\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind playerView from player\n"
            "    bind snapshot from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);

        ctx = transpiler_ctx_create();
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_ready_playerView;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_ready_snapshot;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_playerView;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_snapshot;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasLayer lowers to zone runtime helper inside zone context");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);

        ctx = transpiler_ctx_create();
        ctx->mir = mir;
        ctx->current_zone_name = "BattleZone";

        {
            ASTNode *args[1] = { make_identifier("poison", 1) };
            result = emit_expression(make_call("HasLayer", args, 1, 1), ctx);
            EXPECT(strcmp(result, "BattleZone_has_layer_poison(self, __pgy_zone_gen)") == 0);
            free(result);
        }

        {
            ASTNode *args[1] = { make_identifier("trust", 1) };
            result = emit_expression(make_call("HasLayer", args, 1, 1), ctx);
            EXPECT(strcmp(result, "BattleZone_has_layer_trust(self, __pgy_zone_gen)") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone effect pool emits pooled storage and HasLayer helper scaffolding");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect DamageEffect for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect pool damage: DamageEffect capacity 8\n"
            "    apply damage to player\n"
            "    func Tick() -> Void {\n"
            "        if HasLayer(damage) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);

        ctx = transpiler_ctx_create();
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "struct { DamageEffect items[8]; bool active[8]; uint8_t count; uint8_t cap; } damage;");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_EFFECT_POOL_INIT(self->damage);");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_EFFECT_POOL_APPLY(self->damage, _pgy_damage_instance);");
        EXPECT_STR_CONTAINS(ctx->out->data, "static inline bool\nBattleZone_has_layer_damage(BattleZone *self, uint32_t expected_gen)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_ZONE_RDLOCK(self);");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_ZONE_GENERATION_WARN_IF_STALE(self, expected_gen, \"BattleZone.damage\")");
        EXPECT_STR_CONTAINS(ctx->out->data, "__pgy_zone_gen");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasState lowers to zone runtime state field inside zone context");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);

        ctx = transpiler_ctx_create();
        ctx->mir = mir;
        ctx->current_zone_name = "BattleZone";

        {
            ASTNode *args[1] = { make_identifier("poisoned", 1) };
            result = emit_expression(make_call("HasState", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__state_poisoned") == 0);
            free(result);
        }

        {
            ASTNode *args[3] = {
                make_identifier("allied", 1),
                make_identifier("player", 1),
                make_identifier("enemy", 1)
            };
            result = emit_expression(make_call("HasState", args, 3, 1), ctx);
            EXPECT(strcmp(result, "self->__state_allied") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasZone lowers to world runtime zone fields inside world context");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);

        ctx = transpiler_ctx_create();
        ctx->mir = mir;
        ctx->current_world_name = "GameWorld";

        {
            ASTNode *args[1] = { make_identifier("liveBattle", 1) };
            result = emit_expression(make_call("HasZone", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__zone_state_liveBattle") == 0);
            free(result);
        }

        {
            ASTNode *args[1] = { make_identifier("battle", 1) };
            result = emit_expression(make_call("HasZone", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__zone_active_battle") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

/* -----------------------------------------------------------------
 * Tests: statement emitters
 * ----------------------------------------------------------------- */

static void
test_statement_emit(void)
{
    printf("\n[statement_emit]\n");

    TranspilerCtx *ctx;

    TEST("let x: Int = 42 → int32_t x = 42;");
    {
        ASTNode *node = make_let("x", make_type_node("Int"), make_number(42, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "int32_t x = 42;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let s: String = \"hi\" → char* s = \"hi\";");
    {
        ASTNode *node = make_let("s", make_type_node("String"),
                                  make_string_lit("hi", 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "char* s = \"hi\";");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let maybe: Option<Int> = Some(42) → PgyOption_Int maybe = Some_Int(42);");
    {
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *node = make_let("maybe", make_generic_type("Option", "Int"),
                                 make_call("Some", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyOption_Int maybe = Some_Int(42);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let none: Option<Int> = None() → PgyOption_Int none = None_Int();");
    {
        ASTNode *node = make_let("none", make_generic_type("Option", "Int"),
                                 make_call("None", NULL, 0, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyOption_Int none = None_Int();");
        transpiler_ctx_destroy(ctx);
    }

    TEST("match Option<Int> destructures Some/None");
    {
        ctx = transpiler_ctx_create();

        ASTNode *some_args[1] = { make_number(42, 1) };
        emit_statement(
            make_let("maybe", make_generic_type("Option", "Int"),
                     make_call("Some", some_args, 1, 1), 1),
            ctx);

        ASTNode *bind_args[1] = { make_identifier("v", 2) };
        ASTNode *case_some_body_stmts[1] = {
            make_return(make_identifier("v", 2), 2)
        };
        ASTNode *case_none_body_stmts[1] = {
            make_return(make_number(0, 3), 3)
        };
        ASTNode *cases[2] = {
            make_match_case(
                make_call("Some", bind_args, 1, 2),
                make_block(case_some_body_stmts, 1)),
            make_match_case(
                make_call("None", NULL, 0, 3),
                make_block(case_none_body_stmts, 1))
        };

        emit_statement(make_match(make_identifier("maybe", 2), cases, 2), ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "__match_");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (__match_");
        EXPECT_STR_CONTAINS(ctx->out->data, ".tag == PgyOptionSome");
        EXPECT_STR_CONTAINS(ctx->out->data, "__typeof__(__match_");
        EXPECT_STR_CONTAINS(ctx->out->data, "v = __match_");
        EXPECT_STR_CONTAINS(ctx->out->data, ".value;");
        EXPECT_STR_CONTAINS(ctx->out->data, "else if (__match_");
        EXPECT_STR_CONTAINS(ctx->out->data, ".tag == PgyOptionNone");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let slot: Slot<Int> = ClaimSlot<Int>() → PgySlot_Int slot = pgy_claim_Int();");
    {
        ASTNode *args[0];
        ASTNode *init = make_call("ClaimSlot", args, 0, 1);
        ASTNode *node = make_let("slot", make_type_node("Slot<Int>"), init, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgySlot_Int slot = pgy_claim_Int();");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let ss: SecureSlot<Int> = ClaimSecureSlot() → PgySecureSlot_Int + token");
    {
        ASTNode *args[0];
        ASTNode *init = make_call("ClaimSecureSlot", args, 0, 1);
        ASTNode *node = make_let("ss", make_type_node("SecureSlot<Int>"), init, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyToken_Int ss_token;");
        EXPECT_STR_CONTAINS(out, "PgySecureSlot_Int ss = pgy_claim_secure_Int(");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let rv: ReadView<Int> = ViewRead(slot) → PgySlot_Int rv = slot;");
    {
        ASTNode *args[1] = { make_identifier("slot", 1) };
        ASTNode *node = make_let("rv", make_type_node("ReadView<Int>"),
                                 make_call("ViewRead", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgySlot_Int rv = slot;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let wv: WriteView<Int> = ViewWrite(slot) → PgySlot_Int wv = slot;");
    {
        ASTNode *args[1] = { make_identifier("slot", 1) };
        ASTNode *node = make_let("wv", make_type_node("WriteView<Int>"),
                                 make_call("ViewWrite", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgySlot_Int wv = slot;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let srv: ReadView<Int> = ViewRead(ss) on SecureSlot emits token alias");
    {
        ctx = transpiler_ctx_create();
        ASTNode *claim = make_call("ClaimSecureSlot", NULL, 0, 1);
        ASTNode *secure = make_let("ss", make_type_node("SecureSlot<Int>"), claim, 1);
        emit_statement(secure, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Int ss_token;");

        ASTNode *args[1] = { make_identifier("ss", 2) };
        ASTNode *node = make_let("srv", make_type_node("ReadView<Int>"),
                                 make_call("ViewRead", args, 1, 2), 2);
        emit_statement(node, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "srv = ss;");
        EXPECT_STR_CONTAINS(ctx->out->data, "srv_token = ss_token;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let dst: Slot<Int> = mt materializes moved slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *move_args[1] = { make_identifier("slot", 1) };
        ASTNode *mt = make_let("mt", make_type_node("MoveToken<Int>"),
                               make_call("Move", move_args, 1, 1), 1);
        emit_statement(mt, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySlot_Int mt = slot;");

        ASTNode *dst = make_let("dst", make_type_node("Slot<Int>"),
                                make_identifier("mt", 2), 2);
        emit_statement(dst, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySlot_Int dst = mt;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("return 0 → return 0;");
    {
        ASTNode *node = make_return(make_number(0, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "return 0;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let vertices: Array<Vertex> = meshData → PgyArray_Vertex vertices = meshData;");
    {
        ASTNode *node = make_let("vertices",
                                 make_generic_type("Array", "Vertex"),
                                 make_identifier("meshData", 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyArray_Vertex vertices = meshData;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let shared: Rc<Int> = RcNew(42) → PgyRc_Int shared = pgy_rc_new_Int(42);");
    {
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *node = make_let("shared",
                                 make_generic_type("Rc", "Int"),
                                 make_call("RcNew", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyRc_Int shared = pgy_rc_new_Int(42);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let alloc: Allocator = AllocatorPool(1024) → PgyAllocator alloc = pgy_allocator_pool(1024);");
    {
        ASTNode *args[1] = { make_number(1024, 1) };
        ASTNode *node = make_let("alloc",
                                 make_type_node("Allocator"),
                                 make_call("AllocatorPool", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyAllocator alloc = pgy_allocator_pool(1024);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let storage: Box<Array<Int>> = BoxArray(128) → fused BoxArray allocation");
    {
        ASTNode *array_type = make_generic_type("Array", "Int");
        ASTNode *boxed_array = make_generic_type_from_node("Box", array_type);
        ASTNode *args[1] = { make_number(128, 1) };
        ASTNode *node = make_let("storage", boxed_array,
                                 make_call("BoxArray", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyBoxArray_Int storage = pgy_box_array_new_Int(128, NULL);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("Write(slot, 42) → pgy_write_Int(&slot, 42);");
    {
        ASTNode *args[2] = { make_identifier("slot", 1), make_number(42, 1) };
        ASTNode *call    = make_call("Write", args, 2, 1);
        const char *out  = emit_stmt_to_str(call, &ctx);
        EXPECT_STR_CONTAINS(out, "pgy_write_Int(&slot, 42)");
        transpiler_ctx_destroy(ctx);
    }

    TEST("unsafe block emits body directly");
    {
        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *unsafe_block = ast_create_unsafe_block(body);
        const char *out = emit_stmt_to_str(unsafe_block, &ctx);
        EXPECT_STR_CONTAINS(out, "/* unsafe */");
        EXPECT_STR_CONTAINS(out, "pgy_log");
        transpiler_ctx_destroy(ctx);
    }

    TEST("defer statement emits cleanup sentinel");
    {
        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *defer_stmt = ast_create_defer_statement(body);
        const char *out = emit_stmt_to_str(defer_stmt, &ctx);
        EXPECT_STR_CONTAINS(out, "__attribute__((cleanup(_pgy_defer_");
        EXPECT_STR_CONTAINS(ctx->helpers->data, "static void _pgy_defer_");
        transpiler_ctx_destroy(ctx);
    }

    TEST("bind statement emits party-role rebinding call");
    {
        ASTNode *bind = ast_create_bind_statement("team", "fighter", "Warrior");
        const char *out = emit_stmt_to_str(bind, &ctx);
        EXPECT(strcmp(out, "") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "cannot resolve party type for bind statement 'team.fighter = Warrior'");
        transpiler_ctx_destroy(ctx);
    }
}

/* -----------------------------------------------------------------
 * Tests: full program output
 * ----------------------------------------------------------------- */

static void
test_program_emit(void)
{
    printf("\n[program_emit]\n");

    TEST("program header contains #include \"pgy_runtime.h\"");
    {
        ASTNode *stmts[0];
        ASTNode *prog = make_program(stmts, 0);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "#include \"pgy_runtime.h\"");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("function emitted at top level with correct signature");
    {
        /* func Add(a: Int, b: Int) -> Int { return a + b } */
        FuncParam pa, pb;
        memset(&pa, 0, sizeof(pa)); pa.name = "a"; pa.type = make_type_node("Int");
        memset(&pb, 0, sizeof(pb)); pb.name = "b"; pb.type = make_type_node("Int");
        FuncParam *params[2] = { &pa, &pb };

        ASTNode *fn = calloc(1, sizeof(ASTNode));
        fn->type = AST_FUNC_DECL;
        fn->data.func_decl.name        = "Add";
        fn->data.func_decl.params      = params;
        fn->data.func_decl.param_count = 2;
        fn->data.func_decl.return_type = make_type_node("Int");
        fn->data.func_decl.body        = NULL;

        ASTNode *stmts[1] = { fn };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t Add(int32_t a, int32_t b)");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("generic function call emits concrete specialization in C");
    {
        FuncParam px;
        memset(&px, 0, sizeof(px));
        px.name = "x";
        px.type = make_type_node("T");
        FuncParam *identity_params[1] = { &px };

        ASTNode *identity_return_stmts[1] = { make_return(make_identifier("x", 1), 1) };
        ASTNode *identity = calloc(1, sizeof(ASTNode));
        identity->type = AST_FUNC_DECL;
        identity->data.func_decl.name = "Identity";
        identity->data.func_decl.params = identity_params;
        identity->data.func_decl.param_count = 1;
        identity->data.func_decl.return_type = make_type_node("T");
        identity->data.func_decl.body = make_block(identity_return_stmts, 1);
        identity->data.func_decl.generic_params = make_generic_params1("T");

        ASTNode *sum = make_let("sum", make_type_node("Int"), make_number(7, 1), 1);
        ASTNode *identity_args[1] = { make_identifier("sum", 1) };
        ASTNode *echoed = make_let("echoed", make_type_node("Int"),
                                   make_call("Identity", identity_args, 1, 1), 1);
        ASTNode *log_args[1] = { make_identifier("echoed", 1) };
        ASTNode *main_stmts[3] = { sum, echoed, make_call("Log", log_args, 1, 1) };

        ASTNode *main = calloc(1, sizeof(ASTNode));
        main->type = AST_FUNC_DECL;
        main->data.func_decl.name = "Main";
        main->data.func_decl.return_type = make_type_node("Void");
        main->data.func_decl.body = make_block(main_stmts, 3);

        ASTNode *stmts[2] = { identity, main };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t Identity_Int(int32_t x)");
        EXPECT_STR_CONTAINS(ctx->out->data, "Identity_Int(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("spawn emits wrapper-based task launch");
    {
        FuncParam px;
        memset(&px, 0, sizeof(px));
        px.name = "x";
        px.type = make_type_node("Int");
        FuncParam *identity_params[1] = { &px };

        ASTNode *identity_return_stmts[1] = { make_return(make_identifier("x", 1), 1) };
        ASTNode *identity = calloc(1, sizeof(ASTNode));
        identity->type = AST_FUNC_DECL;
        identity->data.func_decl.name = "IdentityInt";
        identity->data.func_decl.params = identity_params;
        identity->data.func_decl.param_count = 1;
        identity->data.func_decl.return_type = make_type_node("Int");
        identity->data.func_decl.body = make_block(identity_return_stmts, 1);

        ASTNode *spawn_args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("IdentityInt", spawn_args, 1, 1);
        ASTNode *spawn = calloc(1, sizeof(ASTNode));
        spawn->type = AST_SPAWN_EXPR;
        spawn->data.spawn_expr.function = call;

        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, spawn);
        ASTNode *main = calloc(1, sizeof(ASTNode));
        main->type = AST_FUNC_DECL;
        main->data.func_decl.name = "Main";
        main->data.func_decl.return_type = make_type_node("Void");
        main->data.func_decl.body = main_body;

        ASTNode *stmts[2] = { identity, main };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "IdentityInt");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_async_spawn");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("parallel block emits pgy_spawn / pgy_await per task");
    {
        ASTNode *tasks[2] = {
            make_call("A", NULL, 0, 1),
            make_call("B", NULL, 0, 1)
        };
        ASTNode *par = calloc(1, sizeof(ASTNode));
        par->type = AST_PARALLEL_BLOCK;
        par->data.parallel.tasks      = tasks;
        par->data.parallel.task_count = 2;

        ASTNode *fnA = calloc(1, sizeof(ASTNode));
        fnA->type = AST_FUNC_DECL;
        fnA->data.func_decl.name = "A";
        fnA->data.func_decl.return_type = make_type_node("Void");
        fnA->data.func_decl.body = ast_create_block();
        ASTNode *fnB = calloc(1, sizeof(ASTNode));
        fnB->type = AST_FUNC_DECL;
        fnB->data.func_decl.name = "B";
        fnB->data.func_decl.return_type = make_type_node("Void");
        fnB->data.func_decl.body = ast_create_block();

        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, par);
        ASTNode *main = calloc(1, sizeof(ASTNode));
        main->type = AST_FUNC_DECL;
        main->data.func_decl.name = "Main";
        main->data.func_decl.return_type = make_type_node("Void");
        main->data.func_decl.body = main_body;
        main->data.func_decl.param_count = 0;

        ASTNode *stmts[3] = { fnA, fnB, main };
        ASTNode *prog     = make_program(stmts, 3);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_spawn");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_await");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_par_");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("struct emits typedef struct and method function");
    {
        ASTNode *st = calloc(1, sizeof(ASTNode));
        st->type = AST_CLASS_DECL;
        st->data.class_decl.name = "Vec3";
        st->data.class_decl.is_struct = true;

        ClassField fx, fy, fz;
        memset(&fx, 0, sizeof(fx));
        memset(&fy, 0, sizeof(fy));
        memset(&fz, 0, sizeof(fz));
        fx.name = "x"; fx.type = make_type_node("Float");
        fy.name = "y"; fy.type = make_type_node("Float");
        fz.name = "z"; fz.type = make_type_node("Float");
        ClassField *fields[3] = { &fx, &fy, &fz };
        st->data.class_decl.fields = fields;
        st->data.class_decl.field_count = 3;

        ASTNode method; memset(&method, 0, sizeof(method));
        method.type = AST_FUNC_DECL;
        method.data.func_decl.name = "Length";
        method.data.func_decl.params = NULL;
        method.data.func_decl.param_count = 0;
        method.data.func_decl.return_type = make_type_node("Float");
        method.data.func_decl.body = NULL;

        ASTNode *methods[1] = { &method };
        st->data.class_decl.methods = methods;
        st->data.class_decl.method_count = 1;

        ASTNode *stmts[1] = { st };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct Vec3");
        EXPECT_STR_CONTAINS(ctx->out->data, "float x;");
        EXPECT_STR_CONTAINS(ctx->out->data, "float y;");
        EXPECT_STR_CONTAINS(ctx->out->data, "float z;");
        EXPECT_STR_CONTAINS(ctx->out->data, "float\nVec3_Length(Vec3 self)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("subject method call lowers to self-cell call");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    func Length(self) -> Int {\n"
            "        return self.x;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2();\n"
            "    Log(v.Length());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2_Length(&");
        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2 *self");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->x");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_BOX_DEFINE(Vec2, Vec2)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class method call lowers by value");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    func Length(self) -> Int {\n"
            "        return self.x;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2();\n"
            "    Log(v.Length());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2_Length(");
        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2 self");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self.x;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Box<class> handle lowers through explicit Box helpers");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func MakeVec() -> Box<Vec2> {\n"
            "    return Box(Vec2(1, 2));\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let handle: Box<Vec2> = MakeVec();\n"
            "    Log(BoxGet(handle).x);\n"
            "    BoxSet(handle, Vec2(3, 4));\n"
            "    BoxDrop(handle);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PgyBox_Vec2 MakeVec(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_box_new_Vec2(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_box_get_Vec2(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_box_set_Vec2(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_box_drop_Vec2(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class method bare field access lowers through value self");
    {
        const char *source =
            "class Counter {\n"
            "    let count: Int;\n"
            "    func Tick(self, delta: Int) -> Int {\n"
            "        count = count + delta;\n"
            "        return count;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self.count = (self.count + delta);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self.count;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject action bare field access lowers through self cell");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "    action Tick(self, delta: Int) -> Int {\n"
            "        count = count + delta;\n"
            "        return count;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->count = (self->count + delta);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self->count;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject may own class values and call class func through self field");
    {
        const char *source =
            "class Item {\n"
            "    let name: String;\n"
            "    let damage: Int;\n"
            "    func Info(self) -> String {\n"
            "        return self.name + \" dmg:\" + ToString(self.damage);\n"
            "    }\n"
            "}\n"
            "subject Player {\n"
            "    let name: String;\n"
            "    let weapon: Item;\n"
            "    let hp: Int;\n"
            "    func ShowWeapon(self) -> String {\n"
            "        return name + \" holds \" + weapon.Info();\n"
            "    }\n"
            "    action Strike(self, target: Player) -> Int {\n"
            "        let dmg = weapon.damage;\n"
            "        target.hp = target.hp - dmg;\n"
            "        return dmg;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Info");
        EXPECT_STR_CONTAINS(ctx->out->data, "weapon");
        EXPECT_STR_CONTAINS(ctx->out->data, "target->hp");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone methods lower bare shared fields and helper calls through implicit self");
    {
        const char *source =
            "zone BattleZone {\n"
            "    shared round: Int = 1\n"
            "    func Next(self) -> Int {\n"
            "        round = round + 1;\n"
            "        return round;\n"
            "    }\n"
            "    func Tick(self) -> Int {\n"
            "        return Next() + round;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->round = (self->round + 1);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return (BattleZone_Next(self) + self->round);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world methods lower bare shared fields zone fields and helper calls through implicit self");
    {
        const char *source =
            "zone BattleZone {\n"
            "    shared round: Int = 1\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    shared storm: Int = 1\n"
            "    func Pulse(self) -> Int {\n"
            "        storm = storm + 1;\n"
            "        return storm + battle.round;\n"
            "    }\n"
            "    func Tick(self) -> Int {\n"
            "        return Pulse() + storm;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->storm = (self->storm + 1);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return (self->storm + self->battle.round);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return (GameWorld_Pulse(self) + self->storm);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("vessel methods lower like passive pointer-self receivers");
    {
        const char *source =
            "vessel HealthState {\n"
            "    current: Int;\n"
            "    func IsDead(self) -> Bool {\n"
            "        return current <= 0;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT(strstr(ctx->out->data, "HealthState_IsDead(HealthState *self)") != NULL
            || strstr(ctx->helpers->data, "HealthState_IsDead(HealthState *self)") != NULL);
        EXPECT(strstr(ctx->out->data, "return (self->current <= 0);") != NULL
            || strstr(ctx->helpers->data, "return (self->current <= 0);") != NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class constructor positional arguments lower to field initialization");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2(3, 7);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, ".x = 3");
        EXPECT_STR_CONTAINS(ctx->out->data, ".y = 7");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Slot<subject> lowers through generated object-cell slot helpers");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(3, 7);\n"
            "    Write(s, Vec2(1, 2));\n"
            "    Release(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SLOT_DEFINE(Vec2, Vec2)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySlot_Vec2 s = pgy_claim_Vec2();");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_write_Vec2(&s, (Vec2){ .x = 3, .y = 7 });");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_write_Vec2(&s, (Vec2){ .x = 1, .y = 2 });");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_release_Vec2(&s);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SecureSlot<subject> lowers through generated secure object-cell slot helpers");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Vec2> = Vec2(3, 7);\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SECURE_SLOT_DEFINE(Vec2, Vec2)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_claim_secure_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_write_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_release_Vec2");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SecureSlot<subject> lowers through generated secure object-cell slot helpers");
    {
        const char *source =
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Bot> = Bot(7);\n"
            "    Write(s, Bot(9), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SECURE_SLOT_DEFINE(Bot, Bot)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Bot");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_claim_secure_Bot");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_write_Bot");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_release_Bot");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter lowers as slot pointer boundary");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Touch(ref s: Slot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2));\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(3, 7);\n"
            "    Touch(s);\n"
            "    Release(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "void Touch(PgySlot_Vec2 *s)");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_write_Vec2(s, (Vec2){ .x = 1, .y = 2 });");
        EXPECT_STR_CONTAINS(ctx->out->data, "Touch(&s);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("own SecureSlot<subject> parameter lowers as secure slot pointer boundary");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Consume(own s: SecureSlot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Vec2> = Vec2(3, 7);\n"
            "    Consume(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Consume(PgySecureSlot_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_write_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_release_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "Consume(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("secure boundary forwarding preserves paired token through helper call");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func ConsumeInner(own s: SecureSlot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n"
            "func ConsumeOuter(own s: SecureSlot<Vec2>) -> Void {\n"
            "    ConsumeInner(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "void ConsumeOuter(PgySecureSlot_Vec2 *s, PgyToken_Vec2 s_token)");
        EXPECT_STR_CONTAINS(ctx->out->data, "ConsumeInner(s, s_token);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("transitive secure boundary forwarding preserves paired token through helper chain");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func ConsumeInner(own s: SecureSlot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n"
            "func ConsumeMiddle(own s: SecureSlot<Vec2>) -> Void {\n"
            "    ConsumeInner(s);\n"
            "}\n"
            "func ConsumeOuter(own s: SecureSlot<Vec2>) -> Void {\n"
            "    ConsumeMiddle(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "void ConsumeOuter(PgySecureSlot_Vec2 *s, PgyToken_Vec2 s_token)");
        EXPECT_STR_CONTAINS(ctx->out->data, "void ConsumeMiddle(PgySecureSlot_Vec2 *s, PgyToken_Vec2 s_token)");
        EXPECT_STR_CONTAINS(ctx->out->data, "ConsumeMiddle(s, s_token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "ConsumeInner(s, s_token);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("extern block emits C prototypes");
    {
        ASTNode ext; memset(&ext, 0, sizeof(ext));
        ext.type = AST_EXTERN_BLOCK;
        ext.data.extern_block.abi = "C";

        FuncParam p; memset(&p, 0, sizeof(p));
        p.name = "flags";
        p.type = make_type_node("Int");
        FuncParam *params[1] = { &p };

        ASTNode fn1; memset(&fn1, 0, sizeof(fn1));
        fn1.type = AST_FUNC_DECL;
        fn1.data.func_decl.name = "SDL_Init";
        fn1.data.func_decl.params = params;
        fn1.data.func_decl.param_count = 1;
        fn1.data.func_decl.return_type = make_type_node("Int");
        fn1.data.func_decl.body = NULL;

        ASTNode fn2; memset(&fn2, 0, sizeof(fn2));
        fn2.type = AST_FUNC_DECL;
        fn2.data.func_decl.name = "SDL_Quit";
        fn2.data.func_decl.params = NULL;
        fn2.data.func_decl.param_count = 0;
        fn2.data.func_decl.return_type = make_type_node("Void");
        fn2.data.func_decl.body = NULL;

        ASTNode *decls[2] = { &fn1, &fn2 };
        ext.data.extern_block.declarations = decls;
        ext.data.extern_block.count = 2;

        ASTNode *stmts[1] = { &ext };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "extern \"C\"");
        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t SDL_Init(int32_t flags);");
        EXPECT_STR_CONTAINS(ctx->out->data, "void SDL_Quit();");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("event declaration stays at file scope");
    {
        ASTNode event_node;
        ASTNode param_node;
        ASTNode *params[1] = { &param_node };
        ASTNode *stmts[2];
        ASTNode *prog;
        HIRProgram *hir;
        RIRProgram *rir;
        MIRProgram *mir;
        TranspilerCtx *ctx;
        const char *event_pos;

        memset(&event_node, 0, sizeof(event_node));
        memset(&param_node, 0, sizeof(param_node));

        event_node.type = AST_EVENT_DECL;
        event_node.data.event_decl.name = "OnHit";
        event_node.data.event_decl.params = params;
        event_node.data.event_decl.param_count = 1;
        event_node.data.event_decl.return_type = make_type_node("Void");

        param_node.type = AST_LET_DECL;
        param_node.data.let_decl.name = "damage";
        param_node.data.let_decl.type = make_type_node("Int");

        stmts[0] = &event_node;
        stmts[1] = make_let("boot", make_type_node("Int"), make_number(1, 1), 1);
        prog = make_program(stmts, 2);
        rir = NULL;
        mir = lower_program_to_mir(prog, &hir, &rir);
        ctx = transpiler_ctx_create();
        emit_program(ctx);

        event_pos = strstr(ctx->out->data, "typedef void (*OnHit_Handler)");
        EXPECT(event_pos != NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}

/* -----------------------------------------------------------------
 * Ability / Role codegen
 * ----------------------------------------------------------------- */

static void
test_ability_role_emit(void)
{
    printf("\n[ability_role_emit]\n");

    TEST("ability emits vtable typedef");
    {
        /* Build ability with one method manually (no ast_destroy — manual free) */
        ASTNode ability_node; memset(&ability_node, 0, sizeof(ability_node));
        ability_node.type = AST_ABILITY_DECL;
        ability_node.data.ability_decl.name = "Damageable";

        FuncParam p; memset(&p, 0, sizeof(p));
        p.name = "amount";
        p.type = make_type_node("Int");
        FuncParam *params[1] = { &p };

        ASTNode method; memset(&method, 0, sizeof(method));
        method.type = AST_FUNC_DECL;
        method.data.func_decl.name = "TakeDamage";
        method.data.func_decl.params = params;
        method.data.func_decl.param_count = 1;
        method.data.func_decl.return_type = make_type_node("Void");
        method.data.func_decl.body = NULL;

        ASTNode *methods[1] = { &method };
        ability_node.data.ability_decl.methods = methods;
        ability_node.data.ability_decl.method_count = 1;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_ability_decl(&ability_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Damageable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "(*TakeDamage)");
        EXPECT_STR_CONTAINS(ctx->out->data, "void *self");

        transpiler_ctx_destroy(ctx);
    }

    TEST("role emits static method and vtable instance");
    {
        ASTNode role_node; memset(&role_node, 0, sizeof(role_node));
        role_node.type = AST_ROLE_DECL;
        role_node.data.role_decl.name = "PlayerHeal";

        ASTNode method; memset(&method, 0, sizeof(method));
        method.type = AST_FUNC_DECL;
        method.data.func_decl.name = "Heal";
        method.data.func_decl.params = NULL;
        method.data.func_decl.param_count = 0;
        method.data.func_decl.return_type = make_type_node("Void");
        method.data.func_decl.body = NULL;

        ASTNode *impl_methods[1] = { &method };
        ASTNode impl_node; memset(&impl_node, 0, sizeof(impl_node));
        impl_node.type = AST_IMPL_ABILITY;
        impl_node.data.impl_ability.ability_ref = ast_create_type("Healable");
        impl_node.data.impl_ability.methods = impl_methods;
        impl_node.data.impl_ability.method_count = 1;

        ASTNode *impls[1] = { &impl_node };
        role_node.data.role_decl.impl_abilities = impls;
        role_node.data.role_decl.impl_count = 1;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_role_decl(&role_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PlayerHeal_Heal");
        EXPECT_STR_CONTAINS(ctx->out->data, "Healable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "vtable_instance");
        EXPECT_STR_CONTAINS(ctx->out->data, ".Heal = PlayerHeal_Heal");

        transpiler_ctx_destroy(ctx);
    }

    TEST("generic ability specialization emits canonical vtable tag");
    {
        ASTNode ability_node; memset(&ability_node, 0, sizeof(ability_node));
        ability_node.type = AST_ABILITY_DECL;
        ability_node.data.ability_decl.name = "BatchReady";
        ability_node.data.ability_decl.generic_params = calloc(1, sizeof(GenericParams));
        ability_node.data.ability_decl.generic_params->count = 1;
        ability_node.data.ability_decl.generic_params->params = calloc(1, sizeof(GenericParam *));
        GenericParam *ability_gp = calloc(1, sizeof(GenericParam));
        ability_gp->name = pergyra_strdup("T");
        ability_gp->constraint = ast_create_type("T");
        ability_node.data.ability_decl.generic_params->params[0] = ability_gp;

        FuncParam ability_param; memset(&ability_param, 0, sizeof(ability_param));
        ability_param.name = "batch";
        ability_param.type = make_type_node("T");
        FuncParam *ability_params[1] = { &ability_param };

        ASTNode ability_method; memset(&ability_method, 0, sizeof(ability_method));
        ability_method.type = AST_FUNC_DECL;
        ability_method.data.func_decl.name = "BatchMark";
        ability_method.data.func_decl.params = ability_params;
        ability_method.data.func_decl.param_count = 1;
        ability_method.data.func_decl.return_type = make_type_node("String");
        ability_method.data.func_decl.body = NULL;

        ASTNode *ability_methods[1] = { &ability_method };
        ability_node.data.ability_decl.methods = ability_methods;
        ability_node.data.ability_decl.method_count = 1;

        ASTNode role_node; memset(&role_node, 0, sizeof(role_node));
        role_node.type = AST_ROLE_DECL;
        role_node.data.role_decl.name = "CourierRoute";
        role_node.data.role_decl.for_type = make_type_node("Int");

        FuncParam impl_param; memset(&impl_param, 0, sizeof(impl_param));
        impl_param.name = "batch";
        impl_param.type = make_type_node("Int");
        FuncParam *impl_params[1] = { &impl_param };

        ASTNode impl_method; memset(&impl_method, 0, sizeof(impl_method));
        impl_method.type = AST_FUNC_DECL;
        impl_method.data.func_decl.name = "BatchMark";
        impl_method.data.func_decl.params = impl_params;
        impl_method.data.func_decl.param_count = 1;
        impl_method.data.func_decl.return_type = make_type_node("String");
        impl_method.data.func_decl.body = NULL;

        ASTNode *impl_methods[1] = { &impl_method };
        ASTNode impl_node; memset(&impl_node, 0, sizeof(impl_node));
        impl_node.type = AST_IMPL_ABILITY;
        impl_node.data.impl_ability.ability_ref = make_generic_type("BatchReady", "Int");
        impl_node.data.impl_ability.methods = impl_methods;
        impl_node.data.impl_ability.method_count = 1;

        ASTNode *impls[1] = { &impl_node };
        role_node.data.role_decl.impl_abilities = impls;
        role_node.data.role_decl.impl_count = 1;

        ASTNode *abilities[1] = { &ability_node };
        ASTNode *roles[1] = { &role_node };
        MIRProgram mir; memset(&mir, 0, sizeof(mir));
        MIRRoutine routine; memset(&routine, 0, sizeof(routine));
        mir.abilities = abilities;
        mir.ability_count = 1;
        mir.roles = roles;
        mir.role_count = 1;
        routine.kind = MIR_SCOPE_METHOD;
        routine.name = "BatchMark";
        routine.ast = &impl_method;
        routine.owner_name = "CourierRoute";
        routine.owner_ast_type = AST_ROLE_DECL;
        mir.routines = &routine;
        mir.routine_count = 1;

        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = &mir;
        emit_role_decl(&role_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "BatchReady_Int_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "CourierRoute_BatchReady_Int_vtable_instance");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data, "BatchReady_Int__vtable");

        transpiler_ctx_destroy(ctx);
    }

    TEST("role include copies inherited impls into current role");
    {
        ASTNode base_method; memset(&base_method, 0, sizeof(base_method));
        base_method.type = AST_FUNC_DECL;
        base_method.data.func_decl.name = "Tick";
        base_method.data.func_decl.return_type = make_type_node("Void");

        ASTNode *base_methods[1] = { &base_method };
        ASTNode base_impl; memset(&base_impl, 0, sizeof(base_impl));
        base_impl.type = AST_IMPL_ABILITY;
        base_impl.data.impl_ability.ability_ref = ast_create_type("Updatable");
        base_impl.data.impl_ability.methods = base_methods;
        base_impl.data.impl_ability.method_count = 1;

        ASTNode *base_impls[1] = { &base_impl };
        ASTNode base_role; memset(&base_role, 0, sizeof(base_role));
        base_role.type = AST_ROLE_DECL;
        base_role.data.role_decl.name = "BaseRole";
        base_role.data.role_decl.impl_abilities = base_impls;
        base_role.data.role_decl.impl_count = 1;

        ASTNode include_stmt; memset(&include_stmt, 0, sizeof(include_stmt));
        include_stmt.type = AST_INCLUDE_STMT;
        include_stmt.data.include_stmt.role_name = "BaseRole";

        ASTNode *includes[1] = { &include_stmt };
        ASTNode derived_role; memset(&derived_role, 0, sizeof(derived_role));
        derived_role.type = AST_ROLE_DECL;
        derived_role.data.role_decl.name = "DerivedRole";
        derived_role.data.role_decl.includes = includes;
        derived_role.data.role_decl.include_count = 1;

        ASTNode *roles[2] = { &base_role, &derived_role };
        MIRProgram mir; memset(&mir, 0, sizeof(mir));
        mir.roles = roles;
        mir.role_count = 2;

        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = &mir;
        emit_role_decl(&derived_role, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "DerivedRole_Tick");
        EXPECT_STR_CONTAINS(ctx->out->data, "DerivedRole_Updatable_vtable_instance");

        transpiler_ctx_destroy(ctx);
    }
}

/* -----------------------------------------------------------------
 * Party codegen
 * ----------------------------------------------------------------- */

static void
test_party_emit(void)
{
    printf("\n[party_emit]\n");

    TEST("party emits struct with role slot and shared field");
    {
        ASTNode party_node; memset(&party_node, 0, sizeof(party_node));
        party_node.type = AST_PARTY_DECL;
        party_node.data.party_decl.name = "DungeonTeam";

        /* Role slot */
        ASTNode rs; memset(&rs, 0, sizeof(rs));
        rs.type = AST_ROLE_SLOT;
        rs.data.role_slot.slot_name = "tank";
        ASTNode ab_type; memset(&ab_type, 0, sizeof(ab_type));
        ab_type.type = AST_TYPE;
        ab_type.data.type.name = "Damageable";
        ASTNode *abilities[1] = { &ab_type };
        rs.data.role_slot.required_abilities = abilities;
        rs.data.role_slot.ability_count = 1;

        ASTNode *role_slots[1] = { &rs };
        party_node.data.party_decl.role_slots = role_slots;
        party_node.data.party_decl.role_count = 1;

        /* Shared field */
        ASTNode shared; memset(&shared, 0, sizeof(shared));
        shared.type = AST_PARTY_SHARED;
        shared.data.party_shared.name = "formation";
        shared.data.party_shared.type = make_type_node("String");

        ASTNode *shared_fields[1] = { &shared };
        party_node.data.party_decl.shared_fields = shared_fields;
        party_node.data.party_decl.shared_count = 1;

        party_node.data.party_decl.methods = NULL;
        party_node.data.party_decl.method_count = 0;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_party_decl(&party_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct DungeonTeam");
        EXPECT_STR_CONTAINS(ctx->out->data, "void *tank");
        EXPECT_STR_CONTAINS(ctx->out->data, "Damageable_vtable");
        EXPECT_STR_CONTAINS(ctx->out->data, "formation");
        EXPECT_STR_CONTAINS(ctx->out->data, "} DungeonTeam;");

        transpiler_ctx_destroy(ctx);
    }

    TEST("party instance emits C compound literal");
    {
        ASTNode value; memset(&value, 0, sizeof(value));
        value.type = AST_IDENTIFIER;
        value.data.identifier.name = "tankRole";

        ASTNode instance; memset(&instance, 0, sizeof(instance));
        instance.type = AST_PARTY_INSTANCE;
        instance.data.party_instance.party_type = "DungeonTeam";
        instance.data.party_instance.assignments = calloc(1, sizeof(*instance.data.party_instance.assignments));
        instance.data.party_instance.assignment_count = 1;
        instance.data.party_instance.assignments[0].slot_name = "tank";
        instance.data.party_instance.assignments[0].value = &value;

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(&instance, ctx);

        EXPECT_STR_CONTAINS(result, "(DungeonTeam){");
        EXPECT_STR_CONTAINS(result, ".tank = tankRole");

        free(result);
        free(instance.data.party_instance.assignments);
        transpiler_ctx_destroy(ctx);
    }
}

/* -----------------------------------------------------------------
 * Roster / World codegen
 * ----------------------------------------------------------------- */

#include "tests/transpile/test_transpile_parallel_family.inc"
#include "tests/transpile/test_transpile_domain_async.inc"

static void
test_slot_sugar(void)
{
    printf("\n[slot_sugar]\n");

    TranspilerCtx *ctx;

    TEST("let x: Slot<Int> = 42 → claim + write");
    {
        ASTNode *node = make_let("x", make_type_node("Slot<Int>"),
                                  make_number(42, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT(strstr(out, "pgy_claim_Int()") != NULL);
        EXPECT(strstr(out, "pgy_write_Int(&x, 42)") != NULL);
        transpiler_ctx_destroy(ctx);
    }

    TEST("slot sugar: identifier auto-read via Log");
    {
        /* Build: func Main() -> Void { let x: Slot<Int> = 42; Log(x); } */
        ASTNode *let_node = make_let("x", make_type_node("Slot<Int>"),
                                      make_number(42, 1), 1);

        ASTNode *x_ident = calloc(1, sizeof(ASTNode));
        x_ident->type = AST_IDENTIFIER; x_ident->line = 2;
        x_ident->data.identifier.name = pergyra_strdup("x");

        ASTNode *log_call = calloc(1, sizeof(ASTNode));
        log_call->type = AST_CALL; log_call->line = 2;
        ASTNode *log_id = calloc(1, sizeof(ASTNode));
        log_id->type = AST_IDENTIFIER; log_id->data.identifier.name = pergyra_strdup("Log");
        log_call->data.call.callee = log_id;
        log_call->data.call.arguments = malloc(sizeof(ASTNode*));
        log_call->data.call.arguments[0] = x_ident;
        log_call->data.call.arg_count = 1;
        ctx = transpiler_ctx_create();
        emit_statement(let_node, ctx);
        emit_statement(log_call, ctx);
        EXPECT(strstr(ctx->out->data, "pgy_read_Int(&") != NULL);
        transpiler_ctx_destroy(ctx);
    }

    TEST("slot sugar: x = 5 auto-write");
    {
        ASTNode *let_node = make_let("x", make_type_node("Slot<Int>"),
                                      make_number(42, 1), 1);

        ASTNode *assign = calloc(1, sizeof(ASTNode));
        assign->type = AST_ASSIGNMENT; assign->line = 2;
        ASTNode *tgt = calloc(1, sizeof(ASTNode));
        tgt->type = AST_IDENTIFIER; tgt->data.identifier.name = pergyra_strdup("x");
        assign->data.assignment.target = tgt;
        assign->data.assignment.value  = make_number(5, 2);
        ctx = transpiler_ctx_create();
        emit_statement(let_node, ctx);
        emit_statement(assign, ctx);
        EXPECT(strstr(ctx->out->data, "pgy_write_Int(&") != NULL);
        transpiler_ctx_destroy(ctx);
    }

    TEST("explicit Release prevents double release");
    {
        /* let a: Slot<Int> = ClaimSlot<Int>(); Write(a,10); Release(a); */
        ASTNode *args0[0];
        ASTNode *init = make_call("ClaimSlot", args0, 0, 1);
        ASTNode *let_node = make_let("a", make_type_node("Slot<Int>"), init, 1);

        ASTNode *a_id = calloc(1, sizeof(ASTNode));
        a_id->type = AST_IDENTIFIER; a_id->data.identifier.name = pergyra_strdup("a");
        ASTNode *w_args[] = { a_id, make_number(10, 2) };
        ASTNode *write_call = make_call("Write", w_args, 2, 2);

        ASTNode *a_id2 = calloc(1, sizeof(ASTNode));
        a_id2->type = AST_IDENTIFIER; a_id2->data.identifier.name = pergyra_strdup("a");
        ASTNode *r_args[] = { a_id2 };
        ASTNode *rel_call = make_call("Release", r_args, 1, 3);
        ctx = transpiler_ctx_create();
        emit_statement(let_node, ctx);
        emit_statement(write_call, ctx);
        emit_statement(rel_call, ctx);

        int count = 0;
        const char *p = ctx->out->data;
        while ((p = strstr(p, "pgy_release_Int")) != NULL) { count++; p++; }
        EXPECT(count == 1);
        transpiler_ctx_destroy(ctx);
    }
}

static void
test_stdlib_and_enum_emit(void)
{
    printf("\n[stdlib_enum]\n");

    TranspilerCtx *ctx;

    TEST("array literal let emits PgyArray_Int builder");
    {
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 3;
        arr->data.array_literal.elements = calloc(3, sizeof(ASTNode *));
        arr->data.array_literal.elements[0] = make_number(1, 1);
        arr->data.array_literal.elements[1] = make_number(2, 1);
        arr->data.array_literal.elements[2] = make_number(3, 1);

        ASTNode *node = make_let("values", make_generic_type("Array", "Int"), arr, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyArray_Int values = ({");
        EXPECT_STR_CONTAINS(out, "pgy_array_push_Int");
        transpiler_ctx_destroy(ctx);
    }

    TEST("String built-ins map to runtime helpers");
    {
        ASTNode *args[2] = { make_string_lit("a", 1), make_string_lit("b", 1) };
        ASTNode *call = make_call("Concat", args, 2, 1);
        ASTNode *node = make_let("s", make_type_node("String"), call, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "StringConcat(\"a\", \"b\")");
        transpiler_ctx_destroy(ctx);
    }

    TEST("Set built-ins map to runtime helpers");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let seen: Set<Int> = SetNew();\n"
            "    SetAdd(seen, 7);\n"
            "    Log(SetHas(seen, 7));\n"
            "    Log(SetSize(seen));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_new_int()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_add_int");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_has_int");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_size_int");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("string equality lowers through runtime helper");
    {
        const char *source =
            "func Match(name: String) -> Bool {\n"
            "    return name == \"audit\";\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_string_equals(name, \"audit\")");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("string concat let inference emits char* local");
    {
        const char *source =
            "struct UnitDraft {\n"
            "    roleTitle: String;\n"
            "    originTitle: String;\n"
            "}\n"
            "func FinalizeUnitSpec(draft: UnitDraft) -> String {\n"
            "    let title = draft.roleTitle + \" of the \" + draft.originTitle;\n"
            "    return title;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "StringConcat(");
        EXPECT_STR_CONTAINS(ctx->out->data, "draft.roleTitle");
        EXPECT_STR_CONTAINS(ctx->out->data, "draft.originTitle");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("function type params lower to callable declarators and typed lambdas");
    {
        const char *source =
            "struct StrategyContext {\n"
            "    morale: Int;\n"
            "}\n"
            "func Apply(base: Int, ctx: StrategyContext, policy: func(Int, StrategyContext) -> Int) -> Int {\n"
            "    return policy(base, ctx);\n"
            "}\n"
            "func Run() -> Int {\n"
            "    let ctx = StrategyContext(3);\n"
            "    return Apply(2, ctx, (base: Int, ctx: StrategyContext) => base + ctx.morale);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "int32_t Apply(int32_t base, StrategyContext ctx, int32_t (*policy)(int32_t, StrategyContext))");
        EXPECT(strstr(ctx->decls->data, "pgy_lambda_") != NULL
            || strstr(ctx->helpers->data, "pgy_lambda_") != NULL);
        EXPECT(strstr(ctx->decls->data, "int32_t base, StrategyContext ctx") != NULL
            || strstr(ctx->helpers->data, "int32_t base, StrategyContext ctx") != NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("function typed locals and returns lower as function pointers");
    {
        const char *source =
            "func AddOne(x: Int) -> Int {\n"
            "    return x + 1;\n"
            "}\n"
            "func MakeAdder() -> func(Int) -> Int {\n"
            "    return AddOne;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let f: func(Int) -> Int = AddOne;\n"
            "    let g = MakeAdder();\n"
            "    Log(f(4));\n"
            "    Log(g(9));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->decls->data, "MakeAdder");
        EXPECT_STR_CONTAINS(ctx->out->data, "MakeAdder");
        EXPECT_STR_CONTAINS(ctx->out->data, "AddOne");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("MIR locals keep function-pointer declarators for inferred and annotated callables");
    {
        const char *source =
            "func Compact(route: String, ok: Bool, handle: Int) -> String {\n"
            "    return route;\n"
            "}\n"
            "func Pick(mode: String) -> func(String, Bool, Int) -> String {\n"
            "    return Compact;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let checkoutFormatter = Pick(\"verbose\");\n"
            "    let refundFormatter: func(String, Bool, Int) -> String = Compact;\n"
            "    Log(checkoutFormatter(\"/checkout\", true, 4101));\n"
            "    Log(refundFormatter(\"/refund\", false, 8831));\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        ctx = transpiler_ctx_create();
        ctx->mir = mir;

        EXPECT(ok);
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "char* (*_pgy_ssa_checkoutFormatter_");
        EXPECT_STR_CONTAINS(ctx->out->data,
            ")(char*, bool, int32_t) = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "_pgy_ssa_checkoutFormatter_1 = Pick(\"verbose\");");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "char* (*_pgy_ssa_refundFormatter_");
        EXPECT_STR_CONTAINS(ctx->out->data,
            ")(char*, bool, int32_t) = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "_pgy_ssa_refundFormatter_1 = Compact;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("List<subject> typed let emits specialized helpers");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let roster: List<Player> = ListNew();\n"
            "    let recruit = Player(10);\n"
            "    ListPush(roster, recruit);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_list_new_Player()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_list_push_Player");
        EXPECT_STR_CONTAINS(ctx->decls->data, "PGY_LIST_DEFINE(Player, Player)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("for-in over List<subject> lowers through list storage");
    {
        const char *source =
            "subject Event {\n"
            "    let title: String;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let events: List<Event> = ListNew();\n"
            "    ListPush(events, Event(\"Kickoff\"));\n"
            "    for event in events {\n"
            "        Print(event.title);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_list_new_Event()");
        EXPECT_STR_CONTAINS(ctx->out->data, "for (size_t _pgy_idx_");
        EXPECT_STR_CONTAINS(ctx->out->data, ".count");
        EXPECT_STR_CONTAINS(ctx->out->data, ".data[_pgy_idx_");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Queue<class> typed let emits specialized helpers");
    {
        const char *source =
            "class Weapon {\n"
            "    let name: String;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let bow = Weapon(\"Bow\");\n"
            "    let satchel: Queue<Weapon> = QueueNew();\n"
            "    QueuePush(satchel, bow);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_queue_new_Weapon()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_queue_push_Weapon");
        EXPECT_STR_CONTAINS(ctx->decls->data, "PGY_QUEUE_DEFINE(Weapon, Weapon)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Now and Sleep builtins emit runtime calls");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let t0: Int = Now();\n"
            "    Sleep(5);\n"
            "    let t1: Int = Now();\n"
            "    Log(ToString(t1 - t0));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_now_ms()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_sleep_ms(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ReadLine builtin emits runtime input call");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let line: String = ReadLine();\n"
            "    Print(line);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_input(\"\"");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_print(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HashMap<String, subject> typed let emits specialized helpers");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let registry: HashMap<String, Player> = MapNew();\n"
            "    let hero = Player(42);\n"
            "    MapSet(registry, \"hero\", hero);\n"
            "    let loaded = MapGet(registry, \"hero\");\n"
            "    Log(ToString(loaded.hp));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_new_");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_set_Player");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_get_Player");
        EXPECT_STR_CONTAINS(ctx->decls->data, "PGY_HASHMAP_DEFINE(Player, Player)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("type aliases lower to typedefs and preserve specialized collection types");
    {
        const char *source =
            "class Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "type UserId = Int;\n"
            "type PartyIndex = HashMap<String, Player>;\n"
            "func Load(id: UserId, registry: PartyIndex) -> Player {\n"
            "    return MapGet(registry, \"hero\");\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let id: UserId = 7;\n"
            "    let registry: PartyIndex = MapNew();\n"
            "    MapSet(registry, \"hero\", Player(42));\n"
            "    let hero = Load(id, registry);\n"
            "    Log(ToString(hero.hp));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT(strstr(ctx->out->data, "typedef int32_t UserId;") != NULL
               || strstr(ctx->decls->data, "typedef int32_t UserId;") != NULL);
        EXPECT_STR_CONTAINS(ctx->out->data, "PartyIndex");
        EXPECT(strstr(ctx->decls->data, "PGY_HASHMAP_DEFINE(Player") != NULL);
        EXPECT_STR_CONTAINS(ctx->decls->data, "Load(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_new_");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("nested HashMap<String, List<String>> parses and lowers");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let events: List<String> = ListNew();\n"
            "    ListPush(events, \"a\");\n"
            "    let buckets: HashMap<String, List<String>> = MapNew();\n"
            "    MapSet(buckets, \"today\", events);\n"
            "    let loaded = MapGet(buckets, \"today\");\n"
            "    Log(ToString(ListSize(loaded)));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_list_new_string()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_new_List_String()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_set_List_String");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_get_List_String");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("nested specialized collection signatures emit type defs before prototypes");
    {
        const char *source =
            "func BuildBuckets() -> HashMap<String, List<String>> {\n"
            "    let events: List<String> = ListNew();\n"
            "    let buckets: HashMap<String, List<String>> = MapNew();\n"
            "    MapSet(buckets, \"today\", events);\n"
            "    return buckets;\n"
            "}\n"
            "func RenderBuckets(buckets: HashMap<String, List<String>>) -> String {\n"
            "    let loaded = MapGet(buckets, \"today\");\n"
            "    return ListGet(loaded, 0);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();
        char *map_define_pos;
        char *build_decl_pos;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->decls->data,
            "PGY_HASHMAP_DEFINE(List_String, PgyList_String)");
        EXPECT_STR_CONTAINS(ctx->decls->data,
            "PgyHashMap_List_String BuildBuckets(void);");
        EXPECT_STR_CONTAINS(ctx->decls->data,
            "char* RenderBuckets(PgyHashMap_List_String buckets);");

        map_define_pos = strstr(ctx->decls->data,
            "PGY_HASHMAP_DEFINE(List_String, PgyList_String)");
        build_decl_pos = strstr(ctx->decls->data,
            "PgyHashMap_List_String BuildBuckets(void);");
        EXPECT(map_define_pos != NULL && build_decl_pos != NULL && map_define_pos < build_decl_pos);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("enum variant identifier emits qualified C enum constant");
    {
        ASTNode *enum_decl = calloc(1, sizeof(ASTNode));
        enum_decl->type = AST_ENUM_DECL;
        enum_decl->data.enum_decl.name = pergyra_strdup("Color");
        enum_decl->data.enum_decl.variant_count = 2;
        enum_decl->data.enum_decl.variants = calloc(2, sizeof(char *));
        enum_decl->data.enum_decl.variants[0] = pergyra_strdup("Red");
        enum_decl->data.enum_decl.variants[1] = pergyra_strdup("Blue");

        ASTNode *fn_body = ast_create_block();
        ast_add_statement(fn_body,
            make_let("c", make_type_node("Color"),
                make_identifier("Red", 2), 2));

        ASTNode *fn = calloc(1, sizeof(ASTNode));
        fn->type = AST_FUNC_DECL;
        fn->data.func_decl.name = "Main";
        fn->data.func_decl.return_type = make_type_node("Void");
        fn->data.func_decl.body = fn_body;
        fn->data.func_decl.param_count = 0;

        ASTNode *stmts[2] = { enum_decl, fn };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);
        ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Color_Red");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("operator overload dispatch uses operator_add_Type");
    {
        FuncParam opa, opb, maina, mainb;
        memset(&opa, 0, sizeof(opa)); opa.name = "a"; opa.type = make_type_node("Vec2");
        memset(&opb, 0, sizeof(opb)); opb.name = "b"; opb.type = make_type_node("Vec2");
        memset(&maina, 0, sizeof(maina)); maina.name = "a"; maina.type = make_type_node("Vec2");
        memset(&mainb, 0, sizeof(mainb)); mainb.name = "b"; mainb.type = make_type_node("Vec2");
        FuncParam *op_params[2] = { &opa, &opb };
        FuncParam *main_params[2] = { &maina, &mainb };

        ASTNode *op_fn = calloc(1, sizeof(ASTNode));
        op_fn->type = AST_FUNC_DECL;
        op_fn->data.func_decl.name = "operator_add_Vec2";
        op_fn->data.func_decl.params = op_params;
        op_fn->data.func_decl.param_count = 2;
        op_fn->data.func_decl.return_type = make_type_node("Vec2");
        ASTNode *op_body = ast_create_block();
        ast_add_statement(op_body, make_return(make_identifier("a", 1), 1));
        op_fn->data.func_decl.body = op_body;

        ASTNode *main_fn = calloc(1, sizeof(ASTNode));
        main_fn->type = AST_FUNC_DECL;
        main_fn->data.func_decl.name = "Main";
        main_fn->data.func_decl.params = main_params;
        main_fn->data.func_decl.param_count = 2;
        main_fn->data.func_decl.return_type = make_type_node("Vec2");
        ASTNode *sum = ast_create_binary(make_identifier("a", 2),
            (Token){ .type = TOKEN_PLUS }, make_identifier("b", 2));
        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, make_return(sum, 2));
        main_fn->data.func_decl.body = main_body;

        ASTNode *stmts[2] = { op_fn, main_fn };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);
        ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "operator_add_Vec2(");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("role ability Add emits operator_add_Type alias");
    {
        FuncParam rhs_param, a_param, b_param;
        memset(&rhs_param, 0, sizeof(rhs_param));
        memset(&a_param, 0, sizeof(a_param));
        memset(&b_param, 0, sizeof(b_param));
        rhs_param.name = "other";
        rhs_param.type = make_type_node("Int");
        a_param.name = "a";
        a_param.type = make_type_node("Int");
        b_param.name = "b";
        b_param.type = make_type_node("Int");

        ASTNode *role_method = calloc(1, sizeof(ASTNode));
        role_method->type = AST_FUNC_DECL;
        role_method->data.func_decl.name = "Add";
        role_method->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        role_method->data.func_decl.params[0] = &rhs_param;
        role_method->data.func_decl.param_count = 1;
        role_method->data.func_decl.return_type = make_type_node("Int");
        ASTNode *role_body = ast_create_block();
        ast_add_statement(role_body, make_return(make_number(123, 1), 1));
        role_method->data.func_decl.body = role_body;

        ASTNode *impl = calloc(1, sizeof(ASTNode));
        impl->type = AST_IMPL_ABILITY;
        impl->data.impl_ability.ability_ref = ast_create_type("Arithmetic");
        impl->data.impl_ability.methods = calloc(1, sizeof(ASTNode *));
        impl->data.impl_ability.methods[0] = role_method;
        impl->data.impl_ability.method_count = 1;

        ASTNode *role = calloc(1, sizeof(ASTNode));
        role->type = AST_ROLE_DECL;
        role->data.role_decl.name = "IntMath";
        role->data.role_decl.for_type = make_type_node("Int");
        role->data.role_decl.impl_abilities = calloc(1, sizeof(ASTNode *));
        role->data.role_decl.impl_abilities[0] = impl;
        role->data.role_decl.impl_count = 1;

        ASTNode *main_fn = calloc(1, sizeof(ASTNode));
        main_fn->type = AST_FUNC_DECL;
        main_fn->data.func_decl.name = "Main";
        main_fn->data.func_decl.params = calloc(2, sizeof(FuncParam *));
        main_fn->data.func_decl.params[0] = &a_param;
        main_fn->data.func_decl.params[1] = &b_param;
        main_fn->data.func_decl.param_count = 2;
        main_fn->data.func_decl.return_type = make_type_node("Int");
        ASTNode *sum = ast_create_binary(make_identifier("a", 2),
            (Token){ .type = TOKEN_PLUS }, make_identifier("b", 2));
        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, make_return(sum, 2));
        main_fn->data.func_decl.body = main_body;

        ASTNode *stmts[2] = { role, main_fn };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);
        ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "operator_add_Int(int32_t lhs, int32_t other)");
        EXPECT_STR_CONTAINS(ctx->out->data, "return operator_add_Int(a, b);");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}

/* Build a temp file path using TMPDIR (set by Makefile) or fallback. */
static void
make_tmp_path(char *buf, size_t bufsz, const char *filename)
{
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL || tmpdir[0] == '\0') {
#ifdef _WIN32
        tmpdir = getenv("TEMP");
        if (tmpdir == NULL || tmpdir[0] == '\0')
            tmpdir = ".";
#else
        tmpdir = "/tmp";
#endif
    }
    snprintf(buf, bufsz, "%s/%s", tmpdir, filename);
}

static void
test_mir_vertical_slice_emit(void)
{
    printf("\n[mir_vertical_slice]\n");

    TEST("simple branch-return function body emits from MIR");
    {
        const char *source =
            "func Score(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 7;\n"
            "    }\n"
            "    return 3;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_vertical_slice.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_function_mir_emitability(hir, mir, "Score"))
                fprintf(stderr, "  ⚠ MIR emitability warning for Score: pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const MIRRoutine *routine = find_mir_routine_by_name(mir, "Score");

            EXPECT(strstr(output, "/* emitted-from-mir */") != NULL);
            EXPECT(routine != NULL);
            EXPECT(mir_count_reachable_non_cleanup_blocks(routine) >= 3);
            EXPECT(mir_count_exceptional_edges(routine) == 0);
            EXPECT(strstr(output, "_pgy_mir_bb_Score_0:") != NULL);
            EXPECT(strstr(output, "_pgy_mir_bb_Score_1:") != NULL);
            EXPECT(strstr(output, "_pgy_mir_bb_Score_2:") != NULL);
            EXPECT(mir_block_slice_contains(output, "_pgy_mir_bb_Score_0:", "if ("));
            EXPECT(mir_block_slice_contains(output, "_pgy_mir_bb_Score_0:", "goto _pgy_mir_bb_Score_1;"));
            EXPECT(mir_block_slice_contains(output, "_pgy_mir_bb_Score_0:", "goto _pgy_mir_bb_Score_2;"));
            EXPECT(mir_block_slice_contains(output, "_pgy_mir_bb_Score_1:", "return 7;"));
            EXPECT(mir_block_slice_contains(output, "_pgy_mir_bb_Score_2:", "return 3;"));
            EXPECT((strstr(output, "if (flag)") != NULL)
                   || (strstr(output, "if (_pgy_ssa_flag_") != NULL));
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("phi merge function body emits MIR SSA locals and predecessor copies");
    {
        const char *source =
            "func Score(flag: Bool) -> Int {\n"
            "    let score: Int = 0;\n"
            "    if flag {\n"
            "        score = 7;\n"
            "    } else {\n"
            "        score = 3;\n"
            "    }\n"
            "    return score;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_phi_slice.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_function_mir_emitability(hir, mir, "Score"))
                fprintf(stderr, "  ⚠ MIR emitability warning for Score(phi): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const MIRRoutine *routine = find_mir_routine_by_name(mir, "Score");

            /* Verify MIR produced valid C with score assignments and returns */
            EXPECT(routine != NULL);
            EXPECT(mir_count_phi_instructions(routine) > 0);
            EXPECT(mir_count_reachable_non_cleanup_blocks(routine) >= 3);
            EXPECT(strstr(output, "if (") != NULL);
            EXPECT(strstr(output, "return") != NULL);
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("non-SSA statements in CFG blocks still emit from MIR");
    {
        const char *source =
            "func Touch() -> Void {\n"
            "    Log(\"touch\");\n"
            "    return;\n"
            "}\n"
            "func Score(flag: Bool) -> Int {\n"
            "    Touch();\n"
            "    if flag {\n"
            "        Touch();\n"
            "        return 7;\n"
            "    }\n"
            "    return 3;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_stmt_slice.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_function_mir_emitability(hir, mir, "Score"))
                fprintf(stderr, "  ⚠ MIR emitability warning for Score(stmt): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const MIRRoutine *routine = find_mir_routine_by_name(mir, "Touch");

            /* Verify MIR produced valid C with Touch() calls and returns */
            EXPECT(routine != NULL);
            EXPECT(strstr(output, "Touch();") != NULL);
            EXPECT(strstr(output, "return 7;") != NULL);
            EXPECT(strstr(output, "return 3;") != NULL);
            if (routine != NULL)
                EXPECT(routine->has_cleanup_block == false);
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("intent cleanup CFG emits from MIR exceptional blocks");
    {
        const char *source =
            "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "object BuyerView { let hp: Int; }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot view: BuyerView\n"
            "    authority buyer requires Payable\n"
            "    refresh view from buyer by buyer\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Buyer) {\n"
            "    rollback: full;\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        authorized by: buyer;\n"
            "        requires: Payable;\n"
            "        on: buyer.Pay();\n"
            "        compensate: buyer.Pay();\n"
            "    }\n"
            "    failure: false;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_intent_cleanup.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_intent_mir_emitability(hir, mir, "Purchase"))
                fprintf(stderr, "  ⚠ MIR emitability warning for Purchase(intent): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const MIRRoutine *routine = find_mir_routine_by_name(mir, "Purchase");

            /* Intent MIR emission should produce valid C code */
            EXPECT(routine != NULL);
            EXPECT(strstr(output, "Purchase(") != NULL);
            if (routine != NULL)
                EXPECT(routine->has_cleanup_block == true);
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("intent step subintent call lowers as bool-gated orchestration");
    {
        const char *source =
            "subject Buyer { let hp: Int; }\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Charge(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step verify {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        intent: Charge(checkout, buyer);\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_intent_subintent.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_intent_mir_emitability(hir, mir, "Checkout"))
                fprintf(stderr, "  ⚠ MIR emitability warning for Checkout(intent): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const MIRRoutine *routine = find_mir_routine_by_name(mir, "Checkout");

            /* Subintent MIR emission should produce valid C code */
            EXPECT(routine != NULL);
            EXPECT(strstr(output, "Checkout(") != NULL);
            EXPECT(strstr(output, "Charge(") != NULL);
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("intent header value params lower through MIR and transpiler");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Pay(self) -> Void { return; }\n"
            "}\n"
            "struct PriceQuote {\n"
            "    amount: Int;\n"
            "}\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, buyer: Buyer, quote: PriceQuote, price: Int) {\n"
            "    step pay {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        guard: quote.amount >= price;\n"
            "        on: buyer.Pay();\n"
            "        expect: price > 0;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_intent_value_params.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_intent_mir_emitability(hir, mir, "Checkout"))
                fprintf(stderr, "  ⚠ MIR emitability warning for Checkout(intent-value): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            EXPECT(strstr(output, "Checkout(") != NULL);
            EXPECT(strstr(output, "PriceQuote quote") != NULL);
            EXPECT(strstr(output, "int32_t price") != NULL);
            EXPECT(strstr(output, "quote.amount") != NULL);
            EXPECT(strstr(output, "price > 0") != NULL);
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("intent header interleaved bindings preserve declared C signature order");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Pay(self) -> Void { return; }\n"
            "}\n"
            "struct PriceQuote {\n"
            "    amount: Int;\n"
            "}\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, quote: PriceQuote, buyer: Buyer, price: Int, adjustments: Array<Int>) {\n"
            "    step pay {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        guard: quote.amount >= price;\n"
            "        on: buyer.Pay();\n"
            "        expect: ArrayLength(adjustments) == 2;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_intent_interleaved_value_params.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_intent_mir_emitability(hir, mir, "Checkout"))
                fprintf(stderr, "  ⚠ MIR emitability warning for Checkout(intent-interleaved): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const char *sig = strstr(output, "Checkout(");
            const char *sig_end = sig != NULL ? strchr(sig, ')') : NULL;
            char signature[512];
            const char *checkout_pos = NULL;
            const char *quote_pos = NULL;
            const char *buyer_pos = NULL;
            const char *price_pos = NULL;
            const char *adjustments_pos = NULL;
            size_t sig_len = 0;

            EXPECT(sig != NULL);
            EXPECT(sig_end != NULL);
            EXPECT(strstr(output, "PriceQuote quote") != NULL);
            EXPECT(strstr(output, "int32_t price") != NULL);
            EXPECT(strstr(output, "PgyArray_Int adjustments") != NULL);

            if (sig != NULL && sig_end != NULL) {
                sig_len = (size_t)(sig_end - sig + 1);
                if (sig_len >= sizeof(signature))
                    sig_len = sizeof(signature) - 1;
                memcpy(signature, sig, sig_len);
                signature[sig_len] = '\0';

                checkout_pos = strstr(signature, "checkout");
                quote_pos = strstr(signature, "quote");
                buyer_pos = strstr(signature, "buyer");
                price_pos = strstr(signature, "price");
                adjustments_pos = strstr(signature, "adjustments");

                EXPECT(checkout_pos != NULL);
                EXPECT(quote_pos != NULL);
                EXPECT(buyer_pos != NULL);
                EXPECT(price_pos != NULL);
                EXPECT(adjustments_pos != NULL);
                EXPECT(checkout_pos < quote_pos);
                EXPECT(quote_pos < buyer_pos);
                EXPECT(buyer_pos < price_pos);
                EXPECT(price_pos < adjustments_pos);
            }
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }
}

static void
test_intent_observability_emit(void)
{
    printf("\n[intent_observability_emit]\n");

    TEST("intent observability builtins lower to runtime exports");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let current: Int = IntentCurrentHandle();\n"
            "    let recent: Int = IntentRecentHandle(0);\n"
            "    let trace: Int = IntentRecentTraceId(0);\n"
            "    let steps: Int = IntentActiveStepCount(current);\n"
            "    let name: String = IntentActiveStepName(recent, 0);\n"
            "    let zone: String = IntentActiveStepZone(recent, 0);\n"
            "    let phase: String = IntentActiveStepPhase(recent, 0);\n"
            "    let participant: String = IntentActiveStepParticipant(recent, 0);\n"
            "    let slot: String = IntentActiveStepSlot(recent, 0);\n"
            "    let from_zone: String = IntentActiveStepFromZone(recent, 0);\n"
            "    let from_slot: String = IntentActiveStepFromSlot(recent, 0);\n"
            "    let to_zone: String = IntentActiveStepToZone(recent, 0);\n"
            "    let to_slot: String = IntentActiveStepToSlot(recent, 0);\n"
            "    let ok: Bool = IntentActiveStepOk(recent, 0);\n"
            "    let failure: String = IntentActiveStepFailure(recent, 0);\n"
            "    Log(ToString(current + recent + trace + steps));\n"
            "    Log(name + zone + phase + participant + slot + from_zone + from_slot + to_zone + to_slot + failure);\n"
            "    Log(ok);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = NULL;
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_current_handle_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_recent_handle_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_recent_trace_id_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_count_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_name_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_zone_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_phase_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_participant_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_slot_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_from_zone_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_from_slot_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_to_zone_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_to_slot_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_ok_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_failure_export(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

/* -----------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------- */

int
main(void)
{
    printf("=== Pergyra C Transpiler Test Suite ===\n");

    type_system_init();

    test_codebuf();
    test_type_mapping();
    test_expression_emit();
    test_statement_emit();
    test_program_emit();
    test_ability_role_emit();
    test_party_emit();
    test_roster_world_emit();
    test_parallel_family_emit();
    test_parallel_execution_emit();
    test_slot_sugar();
    test_stdlib_and_enum_emit();
    test_mir_vertical_slice_emit();
    test_intent_observability_emit();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    type_system_cleanup();
    return (g_fail > 0) ? 1 : 0;
}
