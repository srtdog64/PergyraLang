#include "lsp/pgy_lsp_internal.h"
#include "lexer/lexer_keywords.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMPLETION_JSON_CAPACITY 65536u
#define COMPLETION_LABEL_CAPACITY 128u
#define COMPLETION_DETAIL_CAPACITY 128u
#define EXPECTED_COMPLETION_COUNT 28u

static const char *
skip_space(const char *cursor)
{
    while (cursor != NULL && *cursor != '\0'
           && isspace((unsigned char)*cursor))
        cursor++;
    return cursor;
}

static bool
consume_char(const char **cursor, char expected)
{
    const char *current;

    if (cursor == NULL)
        return false;
    current = skip_space(*cursor);
    if (current == NULL || *current != expected)
        return false;
    *cursor = current + 1;
    return true;
}

static bool
consume_text(const char **cursor, const char *expected)
{
    size_t length;
    const char *current;

    if (cursor == NULL || expected == NULL)
        return false;
    current = skip_space(*cursor);
    length = strlen(expected);
    if (current == NULL || strncmp(current, expected, length) != 0)
        return false;
    *cursor = current + length;
    return true;
}

static int
hex_digit_value(char digit)
{
    if (digit >= '0' && digit <= '9')
        return digit - '0';
    if (digit >= 'a' && digit <= 'f')
        return digit - 'a' + 10;
    if (digit >= 'A' && digit <= 'F')
        return digit - 'A' + 10;
    return -1;
}

static bool
parse_json_string(const char **cursor, char *out, size_t out_size)
{
    const char *current;
    size_t length = 0;

    if (cursor == NULL || out == NULL || out_size == 0)
        return false;
    current = skip_space(*cursor);
    if (current == NULL || *current != '"')
        return false;
    current++;

    while (*current != '\0' && *current != '"') {
        unsigned char value = (unsigned char)*current++;

        if (value == '\\') {
            int high;
            int low;

            switch (*current++) {
            case '"': value = '"'; break;
            case '\\': value = '\\'; break;
            case 'b': value = '\b'; break;
            case 'f': value = '\f'; break;
            case 'n': value = '\n'; break;
            case 'r': value = '\r'; break;
            case 't': value = '\t'; break;
            case 'u':
                if (current[0] != '0' || current[1] != '0')
                    return false;
                high = hex_digit_value(current[2]);
                low = hex_digit_value(current[3]);
                if (high < 0 || low < 0)
                    return false;
                value = (unsigned char)((high << 4) | low);
                current += 4;
                break;
            default:
                return false;
            }
        } else if (value < 0x20) {
            return false;
        }
        if (length + 1 >= out_size)
            return false;
        out[length++] = (char)value;
    }

    if (*current != '"')
        return false;
    out[length] = '\0';
    *cursor = current + 1;
    return true;
}

static const char *
expected_class_name(PgyLanguageKeywordClass keyword_class)
{
    switch (keyword_class) {
    case PGY_KEYWORD_CLASS_RESERVED: return "reserved";
    case PGY_KEYWORD_CLASS_CONTEXTUAL: return "contextual";
    case PGY_KEYWORD_CLASS_SOFT: return "soft";
    }
    return NULL;
}

static const char *
expected_axis_name(PgyLanguageKeywordAxis axis)
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

static const PgyLanguageKeywordRow *
next_completion_row(size_t *registry_index)
{
    while (*registry_index < lexer_keyword_registry_count()) {
        const PgyLanguageKeywordRow *row =
            lexer_keyword_registry_row((*registry_index)++);
        if (row != NULL
            && (row->tooling_flags & PGY_KEYWORD_TOOLING_COMPLETION) != 0)
            return row;
    }
    return NULL;
}

static bool
parse_and_verify_items(const char *json, size_t *parsed_count_out)
{
    const char *cursor = json;
    size_t registry_index = 0;
    size_t parsed_count = 0;
    bool saw_domain = false;
    bool saw_sync = false;

    if (!consume_char(&cursor, '['))
        return false;
    cursor = skip_space(cursor);
    while (*cursor != ']') {
        const PgyLanguageKeywordRow *row =
            next_completion_row(&registry_index);
        char label[COMPLETION_LABEL_CAPACITY];
        char detail[COMPLETION_DETAIL_CAPACITY];
        char expected_detail[COMPLETION_DETAIL_CAPACITY];
        const char *class_name;
        const char *axis_name;
        int detail_length;

        if (row == NULL || !consume_char(&cursor, '{')
            || !parse_json_string(&cursor, label, sizeof(label))
            || strcmp(label, "label") != 0
            || !consume_char(&cursor, ':')
            || !parse_json_string(&cursor, label, sizeof(label))
            || !consume_char(&cursor, ',')
            || !parse_json_string(&cursor, detail, sizeof(detail))
            || strcmp(detail, "kind") != 0
            || !consume_char(&cursor, ':')
            || !consume_text(&cursor, "14")
            || !consume_char(&cursor, ',')
            || !parse_json_string(&cursor, detail, sizeof(detail))
            || strcmp(detail, "detail") != 0
            || !consume_char(&cursor, ':')
            || !parse_json_string(&cursor, detail, sizeof(detail))
            || !consume_char(&cursor, '}'))
            return false;

        class_name = expected_class_name(row->keyword_class);
        axis_name = expected_axis_name(row->axis);
        if (class_name == NULL || axis_name == NULL
            || strcmp(label, row->spelling) != 0)
            return false;
        detail_length = snprintf(expected_detail, sizeof(expected_detail),
            "Pergyra %s %s keyword", class_name, axis_name);
        if (detail_length < 0
            || (size_t)detail_length >= sizeof(expected_detail)
            || strcmp(detail, expected_detail) != 0)
            return false;

        saw_domain = saw_domain || strcmp(label, "domain") == 0;
        saw_sync = saw_sync || strcmp(label, "sync") == 0;
        parsed_count++;
        cursor = skip_space(cursor);
        if (*cursor == ',') {
            cursor++;
            cursor = skip_space(cursor);
        } else if (*cursor != ']') {
            return false;
        }
    }
    if (!consume_char(&cursor, ']') || *skip_space(cursor) != '\0'
        || next_completion_row(&registry_index) != NULL
        || saw_domain || saw_sync)
        return false;
    *parsed_count_out = parsed_count;
    return true;
}

int
main(void)
{
    char json[COMPLETION_JSON_CAPACITY];
    char tiny[3];
    size_t built_count = 0;
    size_t parsed_count = 0;
    size_t tiny_count = 99;
    const char *cached;

    if (!lsp_build_completion_items_json(json, sizeof(json), &built_count)) {
        fprintf(stderr, "completion JSON build failed\n");
        return 1;
    }
    if (built_count != EXPECTED_COMPLETION_COUNT) {
        fprintf(stderr, "expected %u completion rows, got %zu\n",
                EXPECTED_COMPLETION_COUNT, built_count);
        return 1;
    }
    if (!parse_and_verify_items(json, &parsed_count)
        || parsed_count != built_count) {
        fprintf(stderr, "completion JSON parse/registry projection mismatch\n");
        return 1;
    }

    cached = lsp_completion_items_json();
    if (cached == NULL || strcmp(cached, json) != 0
        || cached != lsp_completion_items_json()) {
        fprintf(stderr, "completion JSON cache is not stable\n");
        return 1;
    }

    if (lsp_build_completion_items_json(tiny, sizeof(tiny), &tiny_count)
        || strcmp(tiny, "[]") != 0 || tiny_count != 0) {
        fprintf(stderr, "completion JSON overflow did not fail closed\n");
        return 1;
    }

    printf("[lsp-completion-registry-probe] ok (%zu items)\n", built_count);
    return 0;
}
