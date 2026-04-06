#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/rir.h"
#include "compiler/mir.h"
#include "compiler/hir.h"
#include "codegen/transpiler.h"

static bool
lower_pipeline_from_source(const char *source,
                           ASTNode **program_out,
                           HIRProgram **hir_out,
                           RIRProgram **rir_out,
                           MIRProgram **mir_out)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        parser_destroy(parser);
        lexer_destroy(lexer);
        return false;
    }
    SemanticResult *sem = semantic_analyze(ast);
    if (sem == NULL || !sem->success) {
        semantic_result_destroy(sem);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return false;
    }
    char *hir_error = NULL;
    HIRProgram *hir = hir_lower(sem->annotated_ast, &hir_error);
    if (hir == NULL) {
        free(hir_error);
        semantic_result_destroy(sem);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return false;
    }
    RIRProgram *rir = rir_lower(hir, &hir_error);
    if (rir == NULL) {
        free(hir_error);
        hir_destroy(hir);
        semantic_result_destroy(sem);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return false;
    }
    MIRProgram *mir = mir_lower(hir, rir, &hir_error);
    if (mir == NULL) {
        free(hir_error);
        rir_destroy(rir);
        hir_destroy(hir);
        semantic_result_destroy(sem);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return false;
    }
    *program_out = sem->annotated_ast;
    *hir_out = hir;
    *rir_out = rir;
    *mir_out = mir;
    semantic_result_destroy(sem);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return true;
}

int main() {
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
    
    if (!lower_pipeline_from_source(source, &program, &hir, &rir, &mir)) {
        printf("Failed to lower pipeline\n");
        return 1;
    }
    
    printf("MIR routines: %zu\n", mir->routine_count);
    for (size_t i = 0; i < mir->routine_count; i++) {
        printf("  Routine %zu: %s (kind=%d)\n", i, mir->routines[i].name, mir->routines[i].kind);
    }
    
    const char *path = "/tmp/pgy_test_mir_output.c";
    TranspileResult *res = transpile_with_mir(hir, mir, path);
    if (res == NULL || !res->success) {
        printf("Transpile failed\n");
        return 1;
    }
    
    // Read and print the output
    FILE *f = fopen(path, "r");
    if (f) {
        char buf[4096];
        while (fgets(buf, sizeof(buf), f)) {
            printf("%s", buf);
        }
        fclose(f);
    }
    
    transpile_result_destroy(res);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
    // Note: program is owned by semantic result and was freed
    
    return 0;
}
