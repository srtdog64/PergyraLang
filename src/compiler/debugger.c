/*
 * pgy debug — Interactive Pergyra debugger
 *
 * Architecture:
 *   1. Parse source to AST
 *   2. Walk AST with a stepping interpreter
 *   3. At each statement, check breakpoints
 *   4. If breakpoint or step mode, pause and accept commands
 *
 * Current: source-level trace debugger (AST-walking)
 * Future: DWARF info + GDB/LLDB integration for compiled binaries
 */

#include "debugger.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../parser/ast.h"
#include "../semantic/semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_BREAKPOINTS 64

typedef struct {
    int breakpoints[MAX_BREAKPOINTS];
    int bp_count;
    bool step_mode;
    bool running;
    int current_line;
    char **source_lines;
    int line_count;
    const char *current_file;
} DebugCtx;

static char **
split_lines(const char *source, int *out_count)
{
    int count = 0;
    const char *p = source;
    while (*p) {
        if (*p == '\n') count++;
        p++;
    }
    count++; /* last line */

    char **lines = calloc((size_t)count + 1, sizeof(char *));
    p = source;
    for (int i = 0; i < count; i++) {
        const char *end = strchr(p, '\n');
        if (!end) end = p + strlen(p);
        size_t len = (size_t)(end - p);
        lines[i] = malloc(len + 1);
        memcpy(lines[i], p, len);
        lines[i][len] = '\0';
        p = (*end == '\n') ? end + 1 : end;
    }
    *out_count = count;
    return lines;
}

static void
show_context(DebugCtx *ctx, int current_line, int radius)
{
    int start = current_line - radius;
    int end = current_line + radius;
    if (start < 1) start = 1;
    if (end > ctx->line_count) end = ctx->line_count;

    for (int i = start; i <= end; i++) {
        const char *marker = (i == current_line) ? " >> " : "    ";
        bool is_bp = false;
        for (int j = 0; j < ctx->bp_count; j++) {
            if (ctx->breakpoints[j] == i) { is_bp = true; break; }
        }
        printf("%s%s%4d | %s\n",
            is_bp ? "*" : " ",
            marker, i,
            (i - 1 < ctx->line_count) ? ctx->source_lines[i - 1] : "");
    }
}

static bool
is_breakpoint(DebugCtx *ctx, int line)
{
    for (int i = 0; i < ctx->bp_count; i++) {
        if (ctx->breakpoints[i] == line) return true;
    }
    return false;
}

static void
list_breakpoints(DebugCtx *ctx)
{
    if (ctx->bp_count == 0) {
        printf("No breakpoints set\n");
        return;
    }
    printf("Breakpoints:\n");
    for (int i = 0; i < ctx->bp_count; i++)
        printf("  %d\n", ctx->breakpoints[i]);
}

static void
clear_breakpoint(DebugCtx *ctx, int line)
{
    for (int i = 0; i < ctx->bp_count; i++) {
        if (ctx->breakpoints[i] == line) {
            for (int j = i; j + 1 < ctx->bp_count; j++)
                ctx->breakpoints[j] = ctx->breakpoints[j + 1];
            ctx->bp_count--;
            printf("Breakpoint cleared at line %d\n", line);
            return;
        }
    }
    printf("No breakpoint set at line %d\n", line);
}

static void
print_backtrace(DebugCtx *ctx)
{
    printf("#0  %s:%d in Main\n",
        ctx->current_file != NULL ? ctx->current_file : "<unknown>",
        ctx->current_line);
}

static void
debug_prompt(DebugCtx *ctx, int current_line)
{
    char cmd[256];
    ctx->current_line = current_line;

    show_context(ctx, current_line, 3);

    while (true) {
        printf("(pgy-debug:%d) ", current_line);
        fflush(stdout);

        if (!fgets(cmd, sizeof(cmd), stdin)) {
            ctx->running = false;
            return;
        }

        /* Strip newline */
        size_t len = strlen(cmd);
        if (len > 0 && cmd[len - 1] == '\n') cmd[len - 1] = '\0';

        if (cmd[0] == '\0' || strcmp(cmd, "n") == 0 || strcmp(cmd, "next") == 0) {
            ctx->step_mode = true;
            return;
        }
        if (strcmp(cmd, "c") == 0 || strcmp(cmd, "continue") == 0) {
            ctx->step_mode = false;
            return;
        }
        if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            ctx->running = false;
            return;
        }
        if (strcmp(cmd, "l") == 0 || strcmp(cmd, "list") == 0) {
            show_context(ctx, current_line, 8);
            continue;
        }
        if (strncmp(cmd, "b ", 2) == 0) {
            int line = atoi(cmd + 2);
            if (line > 0 && ctx->bp_count < MAX_BREAKPOINTS) {
                ctx->breakpoints[ctx->bp_count++] = line;
                printf("Breakpoint set at line %d\n", line);
            }
            continue;
        }
        if (strncmp(cmd, "cl ", 3) == 0 || strncmp(cmd, "clear ", 6) == 0) {
            const char *arg = (cmd[1] == 'l') ? (cmd + 3) : (cmd + 6);
            int line = atoi(arg);
            if (line > 0)
                clear_breakpoint(ctx, line);
            continue;
        }
        if (strcmp(cmd, "info break") == 0
            || strcmp(cmd, "info breakpoints") == 0) {
            list_breakpoints(ctx);
            continue;
        }
        if (strcmp(cmd, "bt") == 0 || strcmp(cmd, "backtrace") == 0) {
            print_backtrace(ctx);
            continue;
        }
        printf("Commands: n(ext), c(ontinue), b <line>, cl <line>, info break, bt, l(ist), q(uit)\n");
    }
}

static void
debug_walk_statements(DebugCtx *ctx, ASTNode *node)
{
    if (!ctx->running || !node) return;

    int line = node->line;

    if (ctx->step_mode || is_breakpoint(ctx, line)) {
        debug_prompt(ctx, line);
        if (!ctx->running) return;
    }

    /* Walk children based on node type */
    switch (node->type) {
    case AST_PROGRAM:
        for (size_t i = 0; i < node->data.program.count; i++)
            debug_walk_statements(ctx, node->data.program.statements[i]);
        break;
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            debug_walk_statements(ctx, node->data.block.statements[i]);
        break;
    case AST_IF_STMT:
        debug_walk_statements(ctx, node->data.if_stmt.then_branch);
        debug_walk_statements(ctx, node->data.if_stmt.else_branch);
        break;
    case AST_WHILE_LOOP:
        debug_walk_statements(ctx, node->data.while_loop.body);
        break;
    case AST_FUNC_DECL:
        if (node->data.func_decl.name &&
            strcmp(node->data.func_decl.name, "Main") == 0) {
            printf("[debug] entering Main()\n");
            debug_walk_statements(ctx, node->data.func_decl.body);
        }
        break;
    default:
        break;
    }
}

static char *
read_file_for_debug(const char *path)
{
    FILE *f = fopen(path, "rb");
    size_t read_len;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    read_len = fread(buf, 1, (size_t)len, f);
    buf[read_len] = '\0';
    fclose(f);
    return buf;
}

int
driver_run_debug_command(int argc, char *argv[])
{
    const char *path = NULL;
    SemanticResult *sem = NULL;
    for (int i = 0; i < argc; i++) {
        if (i == 0 && strcmp(argv[i], "debug") == 0)
            continue;
        if (argv[i][0] != '-') { path = argv[i]; break; }
    }

    if (!path) {
        fprintf(stderr, "Usage: pgy debug <file.pgy>\n");
        return 1;
    }

    char *source = read_file_for_debug(path);
    if (!source) {
        fprintf(stderr, "pgy debug: cannot read '%s'\n", path);
        return 1;
    }

    printf("Pergyra Debugger v0.1 — %s\n", path);
    printf("Commands: n(ext), c(ontinue), b <line>, l(ist), q(uit)\n\n");

    /* Parse */
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);

    if (parser_has_error(parser)) {
        fprintf(stderr, "pgy debug: parse error in '%s'\n", path);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    sem = semantic_analyze(program);
    if (sem == NULL) {
        fprintf(stderr, "pgy debug: out of memory during semantic analysis\n");
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    semantic_result_print(sem);
    if (!sem->success) {
        fprintf(stderr, "pgy debug: semantic analysis failed for '%s'\n", path);
        semantic_result_destroy(sem);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
        free(source);
        return 1;
    }

    /* Setup debug context */
    DebugCtx ctx = {0};
    ctx.step_mode = true;
    ctx.running = true;
    ctx.current_line = 1;
    ctx.current_file = path;
    ctx.source_lines = split_lines(source, &ctx.line_count);

    /* Walk */
    debug_walk_statements(&ctx, program);

    if (ctx.running) {
        printf("\n[debug] program ended.\n");
    }

    /* Cleanup */
    for (int i = 0; i < ctx.line_count; i++)
        free(ctx.source_lines[i]);
    free(ctx.source_lines);
    semantic_result_destroy(sem);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
    free(source);
    return 0;
}
