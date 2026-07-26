/*
 * Hover presentation projection and handler.
 */

#include "pgy_lsp_internal.h"
#include "../lexer/lexer_keywords.h"

#include <stdio.h>
#include <string.h>

typedef enum
{
    LSP_HOVER_LANGUAGE_WORD,
    LSP_HOVER_BUILTIN
} LspHoverEntryKind;

typedef struct
{
    const char *word;
    const char *text;
    LspHoverEntryKind kind;
} LspHoverEntry;

#define LANGUAGE_WORD_HOVER(word_value, text_value) \
    { word_value, text_value, LSP_HOVER_LANGUAGE_WORD },
#define BUILTIN_HOVER(word_value, text_value) \
    { word_value, text_value, LSP_HOVER_BUILTIN },

static const LspHoverEntry k_hover_entries[] = {
#include "lsp_hover_content.def"
};

#undef LANGUAGE_WORD_HOVER
#undef BUILTIN_HOVER

static bool
lsp_language_word_hover_enabled(const char *word)
{
    size_t count = lexer_keyword_registry_count();

    for (size_t index = 0; index < count; index++) {
        const PgyLanguageKeywordRow *row = lexer_keyword_registry_row(index);
        if (row != NULL && strcmp(row->spelling, word) == 0) {
            return (row->tooling_flags & PGY_KEYWORD_TOOLING_HOVER) != 0u;
        }
    }
    return false;
}

static const char *
lsp_hover_text_for_word(const char *word)
{
    if (word == NULL)
        return NULL;

    for (size_t i = 0; i < sizeof(k_hover_entries) / sizeof(k_hover_entries[0]); i++) {
        const LspHoverEntry *entry = &k_hover_entries[i];
        if (strcmp(entry->word, word) != 0)
            continue;
        if (entry->kind == LSP_HOVER_LANGUAGE_WORD
            && !lsp_language_word_hover_enabled(word)) {
            return NULL;
        }
        return entry->text;
    }
    return NULL;
}

void
respond_hover(int id, const char *source_text, int line, int character)
{
    char word[128];
    const char *hover_text;

    if (!extract_word_at_position(source_text, line, character, word, sizeof(word))) {
        lsp_respond(id, "null");
        return;
    }

    hover_text = lsp_hover_text_for_word(word);
    if (hover_text != NULL) {
        char escaped_hover[4096];
        char hover_resp[4608];
        int written;

        json_escape_copy(escaped_hover, sizeof(escaped_hover), hover_text);
        written = snprintf(hover_resp, sizeof(hover_resp),
            "{\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"}}",
            escaped_hover);
        if (written < 0 || (size_t)written >= sizeof(hover_resp)) {
            lsp_respond(id, "null");
            return;
        }
        lsp_respond(id, hover_resp);
    } else {
        lsp_respond(id, "null");
    }
}
