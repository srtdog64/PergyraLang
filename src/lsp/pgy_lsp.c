/*
 * Pergyra Language Server Protocol (LSP) implementation
 *
 * Minimal LSP server that provides:
 *   - Diagnostics (parse errors, semantic errors)
 *   - Hover information (keyword help)
 *
 * Communicates via stdin/stdout JSON-RPC 2.0 with Content-Length headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "../common/string_compat.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../parser/ast.h"
#include "../semantic/semantic.h"

/* ================================================================= */
/* JSON helpers (minimal, no external dependency)                     */
/* ================================================================= */

static char response_buf[65536];

static const char *
json_find_string(const char *json, const char *key)
{
    static char value[4096];
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(json, pattern);
    if (pos == NULL) return NULL;

    pos += strlen(pattern);
    while (*pos == ' ' || *pos == ':' || *pos == '\t') pos++;
    if (*pos == '"') {
        pos++;
        size_t i = 0;
        while (*pos && *pos != '"' && i < sizeof(value) - 1) {
            if (*pos == '\\' && *(pos + 1)) {
                pos++;
                switch (*pos) {
                    case 'n': value[i++] = '\n'; break;
                    case 't': value[i++] = '\t'; break;
                    case '\\': value[i++] = '\\'; break;
                    case '"': value[i++] = '"'; break;
                    default: value[i++] = *pos; break;
                }
            } else {
                value[i++] = *pos;
            }
            pos++;
        }
        value[i] = '\0';
        return value;
    }
    return NULL;
}

static int
json_find_int(const char *json, const char *key)
{
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(json, pattern);
    if (pos == NULL) return -1;
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == ':' || *pos == '\t') pos++;
    return atoi(pos);
}

static void
lsp_send(const char *body)
{
    size_t len = strlen(body);
    fprintf(stdout, "Content-Length: %zu\r\n\r\n%s", len, body);
    fflush(stdout);
}

static void
lsp_respond(int id, const char *result_json)
{
    snprintf(response_buf, sizeof(response_buf),
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, result_json);
    lsp_send(response_buf);
}

static void
lsp_notify(const char *method, const char *params_json)
{
    snprintf(response_buf, sizeof(response_buf),
        "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}",
        method, params_json);
    lsp_send(response_buf);
}

static const char *lsp_completion_items =
    "["
    "{\"label\":\"subject\",\"kind\":14,\"detail\":\"Identity-bearing host type\"},"
    "{\"label\":\"object\",\"kind\":14,\"detail\":\"Passive local projection/state type\"},"
    "{\"label\":\"tobject\",\"kind\":14,\"detail\":\"Boundary transfer type\"},"
    "{\"label\":\"intent\",\"kind\":14,\"detail\":\"Orchestration core declaration\"},"
    "{\"label\":\"parallel\",\"kind\":14,\"detail\":\"Core execution primitive\"},"
    "{\"label\":\"spawn\",\"kind\":14,\"detail\":\"Parallel task creation\"},"
    "{\"label\":\"async\",\"kind\":14,\"detail\":\"Suspension surface\"},"
    "{\"label\":\"await\",\"kind\":14,\"detail\":\"Join/completion surface\"},"
    "{\"label\":\"select\",\"kind\":14,\"detail\":\"Readiness arbitration\"},"
    "{\"label\":\"zone\",\"kind\":14,\"detail\":\"Authority/boundary execution layer\"},"
    "{\"label\":\"roster\",\"kind\":14,\"detail\":\"Party container declaration\"},"
    "{\"label\":\"world\",\"kind\":14,\"detail\":\"Cross-zone orchestration boundary\"},"
    "{\"label\":\"ability\",\"kind\":14,\"detail\":\"Behavior contract\"},"
    "{\"label\":\"role\",\"kind\":14,\"detail\":\"Ability implementation\"},"
    "{\"label\":\"func\",\"kind\":14,\"detail\":\"Function declaration\"},"
    "{\"label\":\"let\",\"kind\":14,\"detail\":\"Variable declaration\"}"
    "]";

static void
json_escape_copy(char *dst, size_t dst_size, const char *src)
{
    size_t di = 0;
    if (dst == NULL || dst_size == 0) return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    for (const char *p = src; *p && di + 2 < dst_size; p++) {
        if (*p == '"' || *p == '\\')
            dst[di++] = '\\';
        dst[di++] = *p;
    }
    dst[di] = '\0';
}

static void
append_document_symbol(char *buf, size_t buf_size, size_t *off,
                       const char *name, int kind, int line)
{
    char escaped[256];
    int n;

    if (buf == NULL || off == NULL || name == NULL || *off >= buf_size)
        return;
    json_escape_copy(escaped, sizeof(escaped), name);
    if (*off > 1 && *off < buf_size - 1)
        buf[(*off)++] = ',';
    n = snprintf(buf + *off, buf_size - *off,
        "{\"name\":\"%s\",\"kind\":%d,"
        "\"range\":{\"start\":{\"line\":%d,\"character\":0},\"end\":{\"line\":%d,\"character\":0}},"
        "\"selectionRange\":{\"start\":{\"line\":%d,\"character\":0},\"end\":{\"line\":%d,\"character\":0}}}",
        escaped, kind, line, line, line, line);
    if (n > 0)
        *off += (size_t)n;
}

static void
respond_document_symbols(int id, const char *source_text)
{
    Lexer *lexer;
    Parser *parser;
    ASTNode *ast;
    char symbols[16384];
    size_t off = 0;

    lexer = lexer_create(source_text);
    if (lexer == NULL) {
        lsp_respond(id, "[]");
        return;
    }
    parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        lsp_respond(id, "[]");
        return;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser) || ast == NULL || ast->type != AST_PROGRAM) {
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        lsp_respond(id, "[]");
        return;
    }

    symbols[off++] = '[';
    for (size_t i = 0; i < ast->data.program.count; i++) {
        ASTNode *stmt = ast->data.program.statements[i];
        int line;
        if (stmt == NULL)
            continue;
        line = stmt->line > 0 ? (int)stmt->line - 1 : 0;
        switch (stmt->type) {
        case AST_FUNC_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.func_decl.name, 12, line);
            break;
        case AST_CLASS_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.class_decl.name, 5, line);
            break;
        case AST_ABILITY_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.ability_decl.name, 11, line);
            break;
        case AST_ROLE_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.role_decl.name, 11, line);
            break;
        case AST_PARTY_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.party_decl.name, 5, line);
            break;
        case AST_ROSTER_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.roster_decl.name, 5, line);
            break;
        case AST_WORLD_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.world_decl.name, 5, line);
            break;
        case AST_RELATION_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.relation_decl.name, 5, line);
            break;
        case AST_EFFECT_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.effect_decl.name, 5, line);
            break;
        case AST_ZONE_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.zone_decl.name, 5, line);
            break;
        case AST_ENUM_DECL:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.enum_decl.name, 10, line);
            break;
        case AST_TYPE_ALIAS:
            append_document_symbol(symbols, sizeof(symbols), &off,
                stmt->data.type_alias.name, 13, line);
            break;
        default:
            break;
        }
    }
    if (off < sizeof(symbols) - 1)
        symbols[off++] = ']';
    symbols[off] = '\0';

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    lsp_respond(id, symbols);
}

static bool
extract_word_at_position(const char *source_text, int line, int character,
                         char *out_word, size_t out_word_size)
{
    const char *p;
    const char *line_start;
    const char *ws;
    const char *we;
    size_t wlen;
    int cur_line = 0;

    if (source_text == NULL || out_word == NULL || out_word_size == 0
        || line < 0 || character < 0) {
        return false;
    }

    p = source_text;
    while (cur_line < line && *p) {
        if (*p == '\n')
            cur_line++;
        p++;
    }
    line_start = p;
    p += character;
    ws = p;
    while (ws > line_start && ((*(ws - 1) >= 'a' && *(ws - 1) <= 'z')
        || (*(ws - 1) >= 'A' && *(ws - 1) <= 'Z')
        || (*(ws - 1) >= '0' && *(ws - 1) <= '9')
        || *(ws - 1) == '_')) {
        ws--;
    }
    we = p;
    while ((*we >= 'a' && *we <= 'z')
        || (*we >= 'A' && *we <= 'Z')
        || (*we >= '0' && *we <= '9')
        || *we == '_') {
        we++;
    }

    wlen = (size_t)(we - ws);
    if (wlen == 0 || wlen >= out_word_size)
        return false;
    memcpy(out_word, ws, wlen);
    out_word[wlen] = '\0';
    return true;
}

static bool
ast_decl_name_and_line(ASTNode *stmt, const char **name_out, int *line_out)
{
    if (name_out == NULL || line_out == NULL || stmt == NULL)
        return false;

    *name_out = NULL;
    *line_out = stmt->line > 0 ? (int)stmt->line - 1 : 0;

    switch (stmt->type) {
    case AST_FUNC_DECL:
        *name_out = stmt->data.func_decl.name;
        return *name_out != NULL;
    case AST_CLASS_DECL:
        *name_out = stmt->data.class_decl.name;
        return *name_out != NULL;
    case AST_ABILITY_DECL:
        *name_out = stmt->data.ability_decl.name;
        return *name_out != NULL;
    case AST_ROLE_DECL:
        *name_out = stmt->data.role_decl.name;
        return *name_out != NULL;
    case AST_PARTY_DECL:
        *name_out = stmt->data.party_decl.name;
        return *name_out != NULL;
    case AST_ROSTER_DECL:
        *name_out = stmt->data.roster_decl.name;
        return *name_out != NULL;
    case AST_WORLD_DECL:
        *name_out = stmt->data.world_decl.name;
        return *name_out != NULL;
    case AST_RELATION_DECL:
        *name_out = stmt->data.relation_decl.name;
        return *name_out != NULL;
    case AST_EFFECT_DECL:
        *name_out = stmt->data.effect_decl.name;
        return *name_out != NULL;
    case AST_ZONE_DECL:
        *name_out = stmt->data.zone_decl.name;
        return *name_out != NULL;
    case AST_ENUM_DECL:
        *name_out = stmt->data.enum_decl.name;
        return *name_out != NULL;
    case AST_TYPE_ALIAS:
        *name_out = stmt->data.type_alias.name;
        return *name_out != NULL;
    default:
        return false;
    }
}

static void
respond_definition(int id, const char *uri, const char *source_text,
                   int line, int character)
{
    Lexer *lexer;
    Parser *parser;
    ASTNode *ast;
    char word[128];

    if (uri == NULL || source_text == NULL
        || !extract_word_at_position(source_text, line, character,
                                     word, sizeof(word))) {
        lsp_respond(id, "null");
        return;
    }

    lexer = lexer_create(source_text);
    if (lexer == NULL) {
        lsp_respond(id, "null");
        return;
    }
    parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        lsp_respond(id, "null");
        return;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser) || ast == NULL || ast->type != AST_PROGRAM) {
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        lsp_respond(id, "null");
        return;
    }

    for (size_t i = 0; i < ast->data.program.count; i++) {
        const char *decl_name = NULL;
        int decl_line = 0;
        char escaped_uri[2048];

        if (!ast_decl_name_and_line(ast->data.program.statements[i], &decl_name, &decl_line))
            continue;
        if (strcmp(decl_name, word) != 0)
            continue;

        json_escape_copy(escaped_uri, sizeof(escaped_uri), uri);
        snprintf(response_buf, sizeof(response_buf),
            "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":"
            "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":0},"
            "\"end\":{\"line\":%d,\"character\":0}}}}",
            id, escaped_uri, decl_line, decl_line);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        lsp_send(response_buf);
        return;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    lsp_respond(id, "null");
}

/* ================================================================= */
/* Diagnostics — parse + semantic check, publish errors              */
/* ================================================================= */

static void
publish_diagnostics(const char *uri, const char *source_text)
{
    /* Parse */
    Lexer *lexer = lexer_create(source_text);
    if (lexer == NULL) return;

    Parser *parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        return;
    }

    ASTNode *ast = parser_parse_program(parser);
    bool parse_err = parser_has_error(parser);
    const char *parse_msg = parse_err ? parser_get_error(parser) : NULL;

    char diag_buf[8192];
    diag_buf[0] = '\0';

    if (parse_err && parse_msg != NULL) {
        /* Extract line from parse error if possible */
        int line = 0;
        const char *at_line = strstr(parse_msg, "at line ");
        if (at_line) line = atoi(at_line + 8) - 1;
        if (line < 0) line = 0;

        /* Escape the message for JSON */
        char escaped[512];
        size_t ei = 0;
        for (const char *p = parse_msg; *p && ei < sizeof(escaped) - 2; p++) {
            if (*p == '"' || *p == '\\') escaped[ei++] = '\\';
            escaped[ei++] = *p;
        }
        escaped[ei] = '\0';

        snprintf(diag_buf, sizeof(diag_buf),
            "{\"range\":{\"start\":{\"line\":%d,\"character\":0},"
            "\"end\":{\"line\":%d,\"character\":100}},"
            "\"severity\":1,\"source\":\"pgy\","
            "\"message\":\"%s\"}", line, line, escaped);
    } else if (ast != NULL) {
        /* Semantic analysis */
        SemanticResult *sem = semantic_analyze(ast);
        if (sem != NULL && !sem->success) {
            size_t off = 0;
            size_t emitted = 0;
            for (size_t i = 0; i < sem->diagnostic_count && emitted < 20; i++) {
                Diagnostic *d = sem->diagnostics[i];
                if (d == NULL) continue;

                if (emitted > 0 && off < sizeof(diag_buf) - 1) {
                    diag_buf[off++] = ',';
                }
                int dline = (int)d->line - 1;
                if (dline < 0) dline = 0;
                int severity = (d->level == DIAG_ERROR) ? 1 : 2;

                char escaped[512];
                size_t ei = 0;
                for (const char *p = d->message;
                     *p && ei < sizeof(escaped) - 2; p++) {
                    if (*p == '"' || *p == '\\') escaped[ei++] = '\\';
                    escaped[ei++] = *p;
                }
                escaped[ei] = '\0';

                int n = snprintf(diag_buf + off, sizeof(diag_buf) - off,
                    "{\"range\":{\"start\":{\"line\":%d,\"character\":0},"
                    "\"end\":{\"line\":%d,\"character\":100}},"
                    "\"severity\":%d,\"source\":\"pgy\","
                    "\"message\":\"%s\"}", dline, dline, severity, escaped);
                if (n > 0) off += (size_t)n;
                emitted++;
            }
        }
        semantic_result_destroy(sem);
    }

    /* Escape URI for JSON */
    char escaped_uri[2048];
    size_t ui = 0;
    for (const char *p = uri; *p && ui < sizeof(escaped_uri) - 2; p++) {
        if (*p == '"' || *p == '\\') escaped_uri[ui++] = '\\';
        escaped_uri[ui++] = *p;
    }
    escaped_uri[ui] = '\0';

    char params[16384];
    snprintf(params, sizeof(params),
        "{\"uri\":\"%s\",\"diagnostics\":[%s]}", escaped_uri, diag_buf);
    lsp_notify("textDocument/publishDiagnostics", params);

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
}

/* ================================================================= */
/* LSP message loop                                                   */
/* ================================================================= */

static char msg_buf[262144]; /* 256KB for messages */

static char *
lsp_read_message(void)
{
    /* Read Content-Length header */
    char header[256];
    int content_length = -1;

    while (fgets(header, sizeof(header), stdin) != NULL) {
        if (strncmp(header, "Content-Length:", 15) == 0) {
            content_length = atoi(header + 15);
        }
        /* Empty line marks end of headers */
        if (strcmp(header, "\r\n") == 0 || strcmp(header, "\n") == 0)
            break;
    }

    if (content_length <= 0 || (size_t)content_length >= sizeof(msg_buf))
        return NULL;

    size_t read_total = 0;
    while (read_total < (size_t)content_length) {
        size_t n = fread(msg_buf + read_total, 1,
                         (size_t)content_length - read_total, stdin);
        if (n == 0) return NULL;
        read_total += n;
    }
    msg_buf[content_length] = '\0';
    return msg_buf;
}

/* Store document contents (simple single-document cache) */
static char doc_uri[2048] = "";
static char *doc_content = NULL;

int
main(void)
{
    /* Disable stdout buffering for LSP */
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);

    while (1) {
        char *msg = lsp_read_message();
        if (msg == NULL)
            break;

        const char *method = json_find_string(msg, "method");
        int id = json_find_int(msg, "id");

        if (method == NULL)
            continue;

        if (strcmp(method, "initialize") == 0) {
            lsp_respond(id,
                "{"
                "\"capabilities\":{"
                    "\"textDocumentSync\":1,"
                    "\"hoverProvider\":true,"
                    "\"completionProvider\":{\"resolveProvider\":false},"
                    "\"documentSymbolProvider\":true,"
                    "\"definitionProvider\":true"
                "},"
                "\"serverInfo\":{\"name\":\"pgy-lsp\",\"version\":\"0.1\"}"
                "}");
        }
        else if (strcmp(method, "initialized") == 0) {
            /* No response needed */
        }
        else if (strcmp(method, "shutdown") == 0) {
            lsp_respond(id, "null");
        }
        else if (strcmp(method, "exit") == 0) {
            break;
        }
        else if (strcmp(method, "textDocument/didOpen") == 0) {
            const char *uri = json_find_string(msg, "uri");
            if (uri) {
                strncpy(doc_uri, uri, sizeof(doc_uri) - 1);
                /* Extract text content */
                const char *text = json_find_string(msg, "text");
                if (text) {
                    free(doc_content);
                    doc_content = pergyra_strdup(text);
                    publish_diagnostics(doc_uri, doc_content);
                }
            }
        }
        else if (strcmp(method, "textDocument/didChange") == 0) {
            /* Full sync mode: text is the full content */
            const char *text = json_find_string(msg, "text");
            if (text) {
                free(doc_content);
                doc_content = pergyra_strdup(text);
                publish_diagnostics(doc_uri, doc_content);
            }
        }
        else if (strcmp(method, "textDocument/hover") == 0) {
            /* Basic hover: return keyword descriptions */
            int line = -1, character = -1;
            const char *pos = strstr(msg, "\"position\"");
            if (pos) {
                line = json_find_int(pos, "line");
                character = json_find_int(pos, "character");
            }

            const char *hover_text = NULL;
            if (doc_content && line >= 0 && character >= 0) {
                /* Find the word at the cursor position */
                const char *p = doc_content;
                int cur_line = 0;
                while (cur_line < line && *p) {
                    if (*p == '\n') cur_line++;
                    p++;
                }
                /* Find word at character offset */
                const char *line_start = p;
                p += character;
                /* Find word boundaries */
                const char *ws = p;
                while (ws > line_start && ((*(ws-1) >= 'a' && *(ws-1) <= 'z')
                    || (*(ws-1) >= 'A' && *(ws-1) <= 'Z')
                    || *(ws-1) == '_')) ws--;
                const char *we = p;
                while ((*we >= 'a' && *we <= 'z')
                    || (*we >= 'A' && *we <= 'Z')
                    || *we == '_') we++;

                char word[128];
                size_t wlen = (size_t)(we - ws);
                if (wlen > 0 && wlen < sizeof(word)) {
                    memcpy(word, ws, wlen);
                    word[wlen] = '\0';

                    if (strcmp(word, "func") == 0)
                        hover_text = "**func** — Function declaration";
                    else if (strcmp(word, "let") == 0)
                        hover_text = "**let** — Variable declaration";
                    else if (strcmp(word, "struct") == 0)
                        hover_text = "**struct** — Value type (passed by value)";
                    else if (strcmp(word, "object") == 0)
                        hover_text = "**object** — Passive state-bearing object type; can react but does not initiate intent";
                    else if (strcmp(word, "tobject") == 0)
                        hover_text = "**tobject** — Transfer object. Boundary transfer data type";
                    else if (strcmp(word, "roster") == 0)
                        hover_text = "**roster** — Party container with capacity constraints. Groups multiple parties (e.g., 4-party dungeon raid)";
                    else if (strcmp(word, "roster") == 0)
                        hover_text = "**roster** — Legacy alias. Use **roster**.";
                    else if (strcmp(word, "class") == 0)
                        hover_text = "**class** — Subject declaration (compatibility keyword)";
                    else if (strcmp(word, "subject") == 0)
                        hover_text = "**subject** — Identity-bearing host type";
                    else if (strcmp(word, "match") == 0)
                        hover_text = "**match** — Pattern matching expression";
                    else if (strcmp(word, "parallel") == 0)
                        hover_text = "**parallel** — Parallel execution block";
                    else if (strcmp(word, "with") == 0)
                        hover_text = "**with** — Scoped slot declaration (auto-release)";
                    else if (strcmp(word, "import") == 0)
                        hover_text = "**import** — Import module";
                    else if (strcmp(word, "Log") == 0)
                        hover_text = "**Log(value)** — Print value with newline\n- If argument is multiline string, it is normalized and emitted via banner-style output";
                    else if (strcmp(word, "LogBlock") == 0)
                        hover_text = "**LogBlock(text)** — Print multiline block text with banner-style normalization";
                    else if (strcmp(word, "LogBanner") == 0)
                        hover_text = "**LogBanner(text)** — Print banner text with indentation-normalized output";
                    else if (strcmp(word, "LogRaw") == 0)
                        hover_text = "**LogRaw(value)** — Print raw string value including embedded escapes/newlines";
                    else if (strcmp(word, "Ok") == 0)
                        hover_text = "**Ok(value)** — Create success Result";
                    else if (strcmp(word, "Err") == 0)
                        hover_text = "**Err(message)** — Create error Result";
                    else if (strcmp(word, "Unwrap") == 0)
                        hover_text = "**Unwrap(result)** — Extract value or panic";
                }
            }

            if (hover_text) {
                char hover_resp[1024];
                snprintf(hover_resp, sizeof(hover_resp),
                    "{\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"}}",
                    hover_text);
                lsp_respond(id, hover_resp);
            } else {
                lsp_respond(id, "null");
            }
        }
        else if (strcmp(method, "textDocument/completion") == 0) {
            lsp_respond(id, lsp_completion_items);
        }
        else if (strcmp(method, "textDocument/documentSymbol") == 0) {
            if (doc_content != NULL)
                respond_document_symbols(id, doc_content);
            else
                lsp_respond(id, "[]");
        }
        else if (strcmp(method, "textDocument/definition") == 0) {
            int line = -1, character = -1;
            const char *pos = strstr(msg, "\"position\"");
            if (pos) {
                line = json_find_int(pos, "line");
                character = json_find_int(pos, "character");
            }
            respond_definition(id, doc_uri, doc_content, line, character);
        }
        else {
            /* Unknown method — respond with null for requests */
            if (id >= 0) {
                lsp_respond(id, "null");
            }
        }
    }

    free(doc_content);
    return 0;
}
