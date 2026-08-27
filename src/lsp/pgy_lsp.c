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
#include <windows.h>
#else
#include <sys/select.h>
#endif

#include "../common/numeric_parse.h"
#include "../common/string_compat.h"
#include "../compiler/path_utils.h"
#include "../runtime/pgy_runtime_observability_schema.h"
#include "pgy_lsp_internal.h"
#include "pgy_lsp_self_host_diagnostics.h"

/*
 * 256KB message cap. Load-bearing survivability invariant (docs/189 C9):
 * a document arriving over LSP is bounded by this cap, and a <=256KB
 * source cannot produce anywhere near the 1M-node AST budget whose
 * exhaustion exit(1)s the process -- so the batch compiler's
 * bounded-refusal path is structurally unreachable from editor input.
 * Oversized messages are drained and skipped (never kill the server);
 * see lsp_read_message.
 */
#define PGY_LSP_MESSAGE_BUFFER_SIZE 262144u /* 256KB for messages */

static void
lsp_copy_string(char *dst, size_t dst_size, const char *src)
{
    pergyra_str_copy(dst, dst_size, src);
}

static void
lsp_uri_from_path(char *dst, size_t dst_size, const char *path)
{
    size_t off = 0;

    if (dst == NULL || dst_size == 0)
        return;
    dst[0] = '\0';
    if (path == NULL)
        return;

    off = (size_t)snprintf(dst, dst_size, "file://");
    if (off >= dst_size) {
        dst[dst_size - 1] = '\0';
        return;
    }

    for (const char *p = path; *p != '\0' && off < dst_size - 1; p++) {
        dst[off++] = (*p == '\\') ? '/' : *p;
    }
    dst[off] = '\0';
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

    if (!saw_content_length)
        return NULL;

    if (content_length >= msg_buf_size) {
        /* A persistent server must survive an oversized message: drain the
         * payload so the stream stays framed, then skip it (docs/189 C9).
         * The old behavior returned NULL, which the main loop treated as
         * EOF -- one large didOpen killed the whole server. */
        char sink[4096];
        size_t remaining = content_length;
        while (remaining > 0) {
            size_t chunk = remaining < sizeof(sink) ? remaining : sizeof(sink);
            size_t n = fread(sink, 1, chunk, stdin);
            if (n == 0)
                return NULL;
            remaining -= n;
        }
        fprintf(stderr,
                "pgy-lsp: skipped a %zu-byte message exceeding the %u-byte cap\n",
                content_length, PGY_LSP_MESSAGE_BUFFER_SIZE);
        msg_buf[0] = '\0';
        return msg_buf;
    }

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

/*
 * True when at least one more client message is already waiting on stdin.
 * Used to coalesce didChange bursts: re-analyzing on every keystroke
 * message is wasted work when the next edit is already queued -- only the
 * last message of a burst pays for diagnostics (docs/189 C9). Falls back
 * to "nothing pending" (analyze every message) when the peek is not
 * supported, which is the previous behavior.
 */
static bool
lsp_stdin_has_pending(void)
{
#ifdef _WIN32
    HANDLE h = (HANDLE)_get_osfhandle(_fileno(stdin));
    DWORD avail = 0;

    if (h == INVALID_HANDLE_VALUE)
        return false;
    if (!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL))
        return false;
    return avail > 0;
#else
    fd_set rfds;
    struct timeval tv = { 0, 0 };

    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    return select(1, &rfds, NULL, NULL, &tv) > 0;
#endif
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
                    const char *uri, const char *text, bool analyze)
{
    if (uri != NULL) {
        lsp_copy_string(doc_uri, doc_uri_size, uri);
    }
    if (text != NULL) {
        free(*doc_content);
        *doc_content = pergyra_strdup(text);
        if (analyze)
            publish_diagnostics(doc_uri, *doc_content);
    }
}

static int
dump_diagnostics_file(const char *path)
{
    char *source = path_read_file(path);
    char uri[2048];
    char params[16384];

    if (source == NULL) {
        fprintf(stderr, "pgy-lsp: failed to read source '%s'\n",
                path != NULL ? path : "(null)");
        return 1;
    }

    lsp_uri_from_path(uri, sizeof(uri), path);
    if (!lsp_build_diagnostics_params(uri, source, params, sizeof(params))) {
        free(source);
        fprintf(stderr, "pgy-lsp: failed to build diagnostics for '%s'\n",
                path != NULL ? path : "(null)");
        return 1;
    }

    printf("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":%s}\n",
           params);
    free(source);
    return 0;
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
main(int argc, char **argv)
{
    char doc_uri[2048] = "";
    char *doc_content = NULL;
    char *msg_buf;
    bool diagnostics_deferred = false;

    if (argc == 4 && strcmp(argv[1], "--native-pipeline") == 0
        && strcmp(argv[2], "--dump-diagnostics") == 0) {
        return dump_diagnostics_file(argv[3]);
    }
    if (argc == 3 && strcmp(argv[1], "--dump-diagnostics") == 0)
        return pgy_lsp_run_self_host_diagnostics(argv[0], argv[2]);
    if (argc > 1) {
        fprintf(stderr,
                "usage: pgy-lsp [--dump-diagnostics <source.pgy>]\n"
                "       pgy-lsp --native-pipeline --dump-diagnostics "
                "<source.pgy>\n");
        return 1;
    }

    msg_buf = malloc(PGY_LSP_MESSAGE_BUFFER_SIZE);
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

        if (diagnostics_deferred
            && strcmp(method, "textDocument/didChange") != 0) {
            publish_diagnostics(doc_uri, doc_content);
            diagnostics_deferred = false;
        }

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
                                uri_copy[0] ? uri_copy : NULL, text, true);
            free(text);
        } else if (strcmp(method, "textDocument/didChange") == 0) {
            char *text = json_find_string_dup(msg, "text");
            bool defer = lsp_stdin_has_pending();
            /* Coalesce bursts: when the next message is already queued,
             * store the text but defer diagnostics to the burst's last
             * message (docs/189 C9). */
            store_document_text(doc_uri, sizeof(doc_uri), &doc_content,
                                NULL, text, !defer);
            diagnostics_deferred = defer;
            free(text);
        } else if (strcmp(method, "textDocument/completion") == 0) {
            lsp_respond(id, lsp_completion_items_json());
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
