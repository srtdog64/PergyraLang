/*
 * Pergyra Language Server Protocol (LSP) implementation.
 *
 * Main owns JSON-RPC dispatch only. Feature handlers live in sibling
 * translation units so tooling conformance does not grow a monolithic owner.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "../common/numeric_parse.h"
#include "../common/string_compat.h"
#include "../runtime/pgy_runtime_observability_schema.h"
#include "pgy_lsp_internal.h"

#define PGY_LSP_MESSAGE_BUFFER_SIZE 262144u /* 256KB for messages */

static void
lsp_copy_string(char *dst, size_t dst_size, const char *src)
{
    pergyra_str_copy(dst, dst_size, src);
}

static char *
lsp_read_message(char *msg_buf, size_t msg_buf_size)
{
    char header[256];
    size_t content_length = 0;
    bool saw_content_length = false;

    if (msg_buf == NULL || msg_buf_size == 0)
        return NULL;

    while (fgets(header, sizeof(header), stdin) != NULL) {
        if (strncmp(header, "Content-Length:", 15) == 0) {
            if (!pgy_parse_size_prefix(header + 15, &content_length))
                return NULL;
            saw_content_length = true;
        }
        if (strcmp(header, "\r\n") == 0 || strcmp(header, "\n") == 0)
            break;
    }

    if (!saw_content_length || content_length >= msg_buf_size)
        return NULL;

    size_t read_total = 0;
    while (read_total < content_length) {
        size_t n = fread(msg_buf + read_total, 1,
                         content_length - read_total, stdin);
        if (n == 0) return NULL;
        read_total += n;
    }
    msg_buf[content_length] = '\0';
    return msg_buf;
}

static void
respond_initialize(int id)
{
    lsp_respond(id,
        "{"
        "\"capabilities\":{"
            "\"textDocumentSync\":1,"
            "\"hoverProvider\":true,"
            "\"completionProvider\":{\"resolveProvider\":false},"
            "\"documentSymbolProvider\":true,"
            "\"definitionProvider\":true,"
            "\"referencesProvider\":true,"
            "\"renameProvider\":true"
        "},"
        "\"serverInfo\":{\"name\":\"pgy-lsp\",\"version\":\"0.1\"},"
        "\"experimental\":{"
            "\"airSchema\":\"pgy.air.graph.v1\","
            "\"observabilitySchema\":\"" PGY_OBSERVABILITY_ABI_SCHEMA "\","
            "\"traceSchema\":\"" PGY_OBSERVABILITY_TRACE_SCHEMA "\","
            "\"observabilitySurfaces\":["
                "\"" PGY_OBSERVABILITY_SURFACE_LAST "\","
                "\"" PGY_OBSERVABILITY_SURFACE_HISTORY "\","
                "\"" PGY_OBSERVABILITY_SURFACE_ACTIVE "\","
                "\"" PGY_OBSERVABILITY_SURFACE_RECENT "\""
            "]"
        "}"
        "}");
}

static void
store_document_text(char *doc_uri, size_t doc_uri_size, char **doc_content,
                    const char *uri, const char *text)
{
    if (uri != NULL) {
        lsp_copy_string(doc_uri, doc_uri_size, uri);
    }
    if (text != NULL) {
        free(*doc_content);
        *doc_content = pergyra_strdup(text);
        publish_diagnostics(doc_uri, *doc_content);
    }
}

static void
dispatch_text_position_request(const char *method, int id, const char *msg,
                               const char *doc_uri, const char *doc_content)
{
    int line = -1;
    int character = -1;
    const char *pos = strstr(msg, "\"position\"");

    if (pos != NULL) {
        line = json_find_int(pos, "line");
        character = json_find_int(pos, "character");
    }

    if (strcmp(method, "textDocument/hover") == 0) {
        respond_hover(id, doc_content, line, character);
    } else if (strcmp(method, "textDocument/definition") == 0) {
        respond_definition(id, doc_uri, doc_content, line, character);
    } else if (strcmp(method, "textDocument/references") == 0) {
        respond_references(id, doc_uri, doc_content, line, character);
    } else {
        char new_name[512];
        bool has_new_name =
            json_find_string_copy(msg, "newName", new_name, sizeof(new_name));
        respond_rename(id, doc_uri, doc_content, line, character,
                       has_new_name ? new_name : NULL);
    }
}

int
main(void)
{
    char doc_uri[2048] = "";
    char *doc_content = NULL;
    char *msg_buf = malloc(PGY_LSP_MESSAGE_BUFFER_SIZE);

    if (msg_buf == NULL)
        return 1;

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);

    while (1) {
        char *msg = lsp_read_message(msg_buf, PGY_LSP_MESSAGE_BUFFER_SIZE);
        if (msg == NULL)
            break;

        char method[128];
        bool has_method =
            json_find_string_copy(msg, "method", method, sizeof(method));
        int id = json_find_int(msg, "id");

        if (!has_method)
            continue;

        if (strcmp(method, "initialize") == 0) {
            respond_initialize(id);
        } else if (strcmp(method, "initialized") == 0) {
            /* No response needed. */
        } else if (strcmp(method, "shutdown") == 0) {
            lsp_respond(id, "null");
        } else if (strcmp(method, "exit") == 0) {
            break;
        } else if (strcmp(method, "textDocument/didOpen") == 0) {
            char uri_copy[2048];
            char *text = json_find_string_dup(msg, "text");
            uri_copy[0] = '\0';
            json_find_string_copy(msg, "uri", uri_copy, sizeof(uri_copy));
            store_document_text(doc_uri, sizeof(doc_uri), &doc_content,
                                uri_copy[0] ? uri_copy : NULL, text);
            free(text);
        } else if (strcmp(method, "textDocument/didChange") == 0) {
            char *text = json_find_string_dup(msg, "text");
            store_document_text(doc_uri, sizeof(doc_uri), &doc_content,
                                NULL, text);
            free(text);
        } else if (strcmp(method, "textDocument/completion") == 0) {
            lsp_respond(id, lsp_completion_items);
        } else if (strcmp(method, "textDocument/documentSymbol") == 0) {
            if (doc_content != NULL)
                respond_document_symbols(id, doc_content);
            else
                lsp_respond(id, "[]");
        } else if (strcmp(method, "textDocument/hover") == 0
            || strcmp(method, "textDocument/definition") == 0
            || strcmp(method, "textDocument/references") == 0
            || strcmp(method, "textDocument/rename") == 0) {
            dispatch_text_position_request(method, id, msg, doc_uri, doc_content);
        } else if (id >= 0) {
            lsp_respond(id, "null");
        }
    }

    free(msg_buf);
    free(doc_content);
    return 0;
}
