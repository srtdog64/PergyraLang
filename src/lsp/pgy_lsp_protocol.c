/*
 * LSP wire helpers and shared text utilities.
 */

#include "pgy_lsp_internal.h"
#include "../common/numeric_parse.h"
#include "../lexer/lexer_keywords.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char lsp_response_buf[65536];

#define LSP_COMPLETION_ITEMS_CAPACITY 65536u
#define LSP_COMPLETION_STRING_INPUT_LIMIT 127u
#define LSP_COMPLETION_ESCAPED_CAPACITY \
    (LSP_COMPLETION_STRING_INPUT_LIMIT * 6u + 1u)

typedef struct {
    char *data;
    size_t size;
    size_t length;
} LspCompletionJsonBuilder;

static bool
lsp_completion_append(LspCompletionJsonBuilder *builder,
                      const char *text, size_t length)
{
    if (builder == NULL || builder->data == NULL || text == NULL
        || builder->length >= builder->size
        || length >= builder->size - builder->length)
        return false;
    memcpy(builder->data + builder->length, text, length);
    builder->length += length;
    builder->data[builder->length] = '\0';
    return true;
}

static bool
lsp_completion_append_json_string(LspCompletionJsonBuilder *builder,
                                  const char *text)
{
    char escaped[LSP_COMPLETION_ESCAPED_CAPACITY];
    size_t text_length;
    size_t escaped_length;

    if (text == NULL)
        return false;
    text_length = strlen(text);
    if (text_length > LSP_COMPLETION_STRING_INPUT_LIMIT)
        return false;

    /* json_escape_copy can expand one input byte to at most six bytes. The
     * fixed 6x+NUL buffer therefore cannot truncate any admitted string. */
    json_escape_copy(escaped, sizeof(escaped), text);
    escaped_length = strlen(escaped);
    return lsp_completion_append(builder, "\"", 1)
        && lsp_completion_append(builder, escaped, escaped_length)
        && lsp_completion_append(builder, "\"", 1);
}

static const char *
lsp_completion_class_name(PgyLanguageKeywordClass keyword_class)
{
    switch (keyword_class) {
    case PGY_KEYWORD_CLASS_RESERVED: return "reserved";
    case PGY_KEYWORD_CLASS_CONTEXTUAL: return "contextual";
    case PGY_KEYWORD_CLASS_SOFT: return "soft";
    }
    return NULL;
}

static const char *
lsp_completion_axis_name(PgyLanguageKeywordAxis axis)
{
    switch (axis) {
    case PGY_KEYWORD_AXIS_GENERAL: return "general";
    case PGY_KEYWORD_AXIS_RESOURCE: return "resource";
    case PGY_KEYWORD_AXIS_EXECUTION: return "execution";
    case PGY_KEYWORD_AXIS_DOMAIN: return "domain";
    case PGY_KEYWORD_AXIS_TYPE_CONTRACT: return "type-contract";
    }
    return NULL;
}

static bool
lsp_completion_row_valid(const PgyLanguageKeywordRow *row)
{
    const uint32_t known_contexts =
        PGY_KEYWORD_CONTEXT_DECLARATION | PGY_KEYWORD_CONTEXT_STATEMENT |
        PGY_KEYWORD_CONTEXT_EXPRESSION | PGY_KEYWORD_CONTEXT_TYPE |
        PGY_KEYWORD_CONTEXT_CLAUSE | PGY_KEYWORD_CONTEXT_MODULE |
        PGY_KEYWORD_CONTEXT_INTENT_STEP | PGY_KEYWORD_CONTEXT_ZONE_BODY |
        PGY_KEYWORD_CONTEXT_NAME;
    const uint32_t known_support =
        PGY_KEYWORD_SUPPORT_NATIVE | PGY_KEYWORD_SUPPORT_SELF_HOST;
    const uint32_t known_tooling =
        PGY_KEYWORD_TOOLING_COMPLETION | PGY_KEYWORD_TOOLING_HOVER |
        PGY_KEYWORD_TOOLING_HIGHLIGHT;

    if (row == NULL || row->spelling == NULL || row->spelling[0] == '\0'
        || row->debug_identity == NULL || row->debug_identity[0] == '\0'
        || lsp_completion_class_name(row->keyword_class) == NULL
        || lsp_completion_axis_name(row->axis) == NULL
        || row->context_mask == 0 || (row->context_mask & ~known_contexts) != 0
        || (row->implementation_support & ~known_support) != 0
        || (row->tooling_flags & ~known_tooling) != 0)
        return false;

    if (row->keyword_class == PGY_KEYWORD_CLASS_RESERVED)
        return row->token_type != PGY_KEYWORD_TOKEN_NONE;
    return row->token_type == PGY_KEYWORD_TOKEN_NONE;
}

static bool
lsp_completion_fail(char *out, size_t out_size, size_t *item_count_out)
{
    if (item_count_out != NULL)
        *item_count_out = 0;
    if (out != NULL && out_size >= 3) {
        out[0] = '[';
        out[1] = ']';
        out[2] = '\0';
    } else if (out != NULL && out_size > 0) {
        out[0] = '\0';
    }
    return false;
}

bool
lsp_build_completion_items_json(char *out, size_t out_size,
                                size_t *item_count_out)
{
    LspCompletionJsonBuilder builder = { out, out_size, 0 };
    const char *previous_spelling = NULL;
    size_t item_count = 0;
    size_t row_index;

    if (item_count_out != NULL)
        *item_count_out = 0;
    if (out == NULL || out_size < 3)
        return lsp_completion_fail(out, out_size, item_count_out);
    out[0] = '\0';
    if (!lsp_completion_append(&builder, "[", 1))
        return lsp_completion_fail(out, out_size, item_count_out);

    for (row_index = 0; row_index < lexer_keyword_registry_count(); row_index++) {
        const PgyLanguageKeywordRow *row =
            lexer_keyword_registry_row(row_index);
        const char *class_name;
        const char *axis_name;
        char detail[96];
        int detail_length;

        if (!lsp_completion_row_valid(row)
            || (previous_spelling != NULL
                && strcmp(previous_spelling, row->spelling) >= 0))
            return lsp_completion_fail(out, out_size, item_count_out);
        previous_spelling = row->spelling;

        if ((row->tooling_flags & PGY_KEYWORD_TOOLING_COMPLETION) == 0)
            continue;
        class_name = lsp_completion_class_name(row->keyword_class);
        axis_name = lsp_completion_axis_name(row->axis);
        detail_length = snprintf(detail, sizeof(detail),
            "Pergyra %s %s keyword", class_name, axis_name);
        if (detail_length < 0 || (size_t)detail_length >= sizeof(detail)
            || (item_count > 0
                && !lsp_completion_append(&builder, ",", 1))
            || !lsp_completion_append(&builder, "{\"label\":",
                                      sizeof("{\"label\":") - 1)
            || !lsp_completion_append_json_string(&builder, row->spelling)
            || !lsp_completion_append(&builder, ",\"kind\":14,\"detail\":",
                                      sizeof(",\"kind\":14,\"detail\":") - 1)
            || !lsp_completion_append_json_string(&builder, detail)
            || !lsp_completion_append(&builder, "}", 1))
            return lsp_completion_fail(out, out_size, item_count_out);
        item_count++;
    }

    if (!lsp_completion_append(&builder, "]", 1))
        return lsp_completion_fail(out, out_size, item_count_out);
    if (item_count_out != NULL)
        *item_count_out = item_count;
    return true;
}

const char *
lsp_completion_items_json(void)
{
    static char items[LSP_COMPLETION_ITEMS_CAPACITY] = "[]";
    static bool initialized = false;

    /* pgy-lsp dispatch is single-threaded; the immutable registry is projected
     * once and the same bounded JSON value is reused for later requests. */
    if (!initialized) {
        initialized = true;
        (void)lsp_build_completion_items_json(items, sizeof(items), NULL);
    }
    return items;
}

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
    static const char hex[] = "0123456789abcdef";
    size_t di = 0;

    if (dst == NULL || dst_size == 0) return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    for (const unsigned char *p = (const unsigned char *)src; *p != '\0'; p++) {
        const char *escape = NULL;
        char unicode_escape[6];
        size_t escape_len = 2;

        switch (*p) {
        case '"': escape = "\\\""; break;
        case '\\': escape = "\\\\"; break;
        case '\b': escape = "\\b"; break;
        case '\f': escape = "\\f"; break;
        case '\n': escape = "\\n"; break;
        case '\r': escape = "\\r"; break;
        case '\t': escape = "\\t"; break;
        default:
            if (*p >= 0x20) {
                unicode_escape[0] = (char)*p;
                escape = unicode_escape;
                escape_len = 1;
                break;
            }
            unicode_escape[0] = '\\';
            unicode_escape[1] = 'u';
            unicode_escape[2] = '0';
            unicode_escape[3] = '0';
            unicode_escape[4] = hex[*p >> 4];
            unicode_escape[5] = hex[*p & 0x0f];
            escape = unicode_escape;
            escape_len = sizeof(unicode_escape);
            break;
        }

        if (escape_len >= dst_size - di)
            break;
        memcpy(dst + di, escape, escape_len);
        di += escape_len;
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
