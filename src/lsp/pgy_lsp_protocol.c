/*
 * LSP wire helpers and shared text utilities.
 */

#include "pgy_lsp_internal.h"
#include "../common/numeric_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char lsp_response_buf[65536];

const char *lsp_completion_items =
    "["
    "{\"label\":\"subject\",\"kind\":14,\"detail\":\"Identity-bearing host type\"},"
    "{\"label\":\"action\",\"kind\":14,\"detail\":\"Subject-host contract-bearing operation\"},"
    "{\"label\":\"object\",\"kind\":14,\"detail\":\"Passive local projection/state type\"},"
    "{\"label\":\"tobject\",\"kind\":14,\"detail\":\"Boundary transfer type\"},"
    "{\"label\":\"intent\",\"kind\":14,\"detail\":\"Orchestration core declaration\"},"
    "{\"label\":\"where\",\"kind\":14,\"detail\":\"Type/step zone contract clause\"},"
    "{\"label\":\"who\",\"kind\":14,\"detail\":\"Intent step participant clause\"},"
    "{\"label\":\"using\",\"kind\":14,\"detail\":\"Intent step bound-zone alias clause\"},"
    "{\"label\":\"transfer\",\"kind\":14,\"detail\":\"Cross-zone handoff clause with using/where derivation\"},"
    "{\"label\":\"authority\",\"kind\":14,\"detail\":\"Zone mutation authority declaration\"},"
    "{\"label\":\"requires\",\"kind\":14,\"detail\":\"Ability contract clause\"},"
    "{\"label\":\"within\",\"kind\":14,\"detail\":\"Action zone contract clause\"},"
    "{\"label\":\"causes\",\"kind\":14,\"detail\":\"Effect contract clause\"},"
    "{\"label\":\"authorized\",\"kind\":14,\"detail\":\"Authority contract clause head\"},"
    "{\"label\":\"with\",\"kind\":14,\"detail\":\"Scoped binding or declaration-local effects clause head\"},"
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

static const char *
json_find_string_start(const char *json, const char *key)
{
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(json, pattern);
    if (pos == NULL) return NULL;

    pos += strlen(pattern);
    while (*pos == ' ' || *pos == ':' || *pos == '\t') pos++;
    return *pos == '"' ? pos + 1 : NULL;
}

static size_t
json_copy_string_payload(const char *pos, char *out, size_t out_size)
{
    size_t i = 0;
    if (out != NULL && out_size > 0)
        out[0] = '\0';
    if (pos == NULL)
        return 0;

    while (*pos && *pos != '"') {
        char ch = *pos;
        if (ch == '\\' && *(pos + 1)) {
            pos++;
            switch (*pos) {
                case 'n': ch = '\n'; break;
                case 't': ch = '\t'; break;
                case '\\': ch = '\\'; break;
                case '"': ch = '"'; break;
                default: ch = *pos; break;
            }
        }
        if (out != NULL && out_size > 0 && i + 1 < out_size)
            out[i] = ch;
        i++;
        pos++;
    }
    if (out != NULL && out_size > 0)
        out[i < out_size ? i : out_size - 1] = '\0';
    return i;
}

bool
json_find_string_copy(const char *json, const char *key,
                      char *out, size_t out_size)
{
    const char *pos = json_find_string_start(json, key);
    if (pos == NULL)
        return false;
    json_copy_string_payload(pos, out, out_size);
    return true;
}

char *
json_find_string_dup(const char *json, const char *key)
{
    const char *pos = json_find_string_start(json, key);
    if (pos == NULL)
        return NULL;

    size_t len = json_copy_string_payload(pos, NULL, 0);
    char *value = malloc(len + 1);
    if (value == NULL)
        return NULL;
    json_copy_string_payload(pos, value, len + 1);
    return value;
}

int
json_find_int(const char *json, const char *key)
{
    char pattern[128];
    int parsed;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(json, pattern);
    if (pos == NULL)
        return -1;
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == ':' || *pos == '\t')
        pos++;
    if (!pgy_parse_int_prefix(pos, &parsed))
        return -1;
    return parsed;
}

void
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

void
lsp_send(const char *body)
{
    size_t len = strlen(body);
    fprintf(stdout, "Content-Length: %zu\r\n\r\n%s", len, body);
    fflush(stdout);
}

void
lsp_respond(int id, const char *result_json)
{
    snprintf(lsp_response_buf, sizeof(lsp_response_buf),
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, result_json);
    lsp_send(lsp_response_buf);
}

void
lsp_notify(const char *method, const char *params_json)
{
    snprintf(lsp_response_buf, sizeof(lsp_response_buf),
        "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}",
        method, params_json);
    lsp_send(lsp_response_buf);
}

bool
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

bool
is_word_boundary_char(char c)
{
    return !((c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9')
        || c == '_');
}
