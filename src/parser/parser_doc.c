#include "parser_internal.h"
#include <ctype.h>

static char *
parser_trimmed_copy(const char *text)
{
    const char *start = text;
    const char *end;
    size_t length;

    if (text == NULL)
        return pergyra_strdup("");

    while (*start != '\0' && isspace((unsigned char)*start))
        start++;

    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1]))
        end--;

    length = (size_t)(end - start);
    return pergyra_strndup(start, length);
}

static bool
parser_doc_tag_name_equals(const char *name, const char *expected)
{
    size_t i;

    if (name == NULL || expected == NULL)
        return false;

    for (i = 0; name[i] != '\0' && expected[i] != '\0'; i++) {
        if (tolower((unsigned char)name[i]) != tolower((unsigned char)expected[i]))
            return false;
    }

    return name[i] == '\0' && expected[i] == '\0';
}

static bool
parser_doc_tag_type_from_name(const char *name, DocTagType *out_type)
{
    if (parser_doc_tag_name_equals(name, "what")) {
        *out_type = DOC_TAG_WHAT;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "why")) {
        *out_type = DOC_TAG_WHY;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "alt")) {
        *out_type = DOC_TAG_ALT;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "next")) {
        *out_type = DOC_TAG_NEXT;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "effects")) {
        *out_type = DOC_TAG_EFFECTS;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "params")) {
        *out_type = DOC_TAG_PARAMS;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "returns")) {
        *out_type = DOC_TAG_RETURNS;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "throws")) {
        *out_type = DOC_TAG_THROWS;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "complexity")) {
        *out_type = DOC_TAG_COMPLEXITY;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "invariants")) {
        *out_type = DOC_TAG_INVARIANTS;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "example")) {
        *out_type = DOC_TAG_EXAMPLE;
        return true;
    }

    return false;
}

static StructuredComment *
parser_ensure_pending_doc_comment(Parser *parser)
{
    if (parser->pending_doc_comment == NULL)
        parser->pending_doc_comment = calloc(1, sizeof(StructuredComment));
    return parser->pending_doc_comment;
}

static bool
parser_append_doc_tag(StructuredComment *comment, DocTag *tag)
{
    DocTag **new_tags;
    size_t next_count;

    if (comment == NULL)
        return false;

    next_count = comment->tag_count + 1;
    new_tags = realloc(comment->tags, next_count * sizeof(DocTag *));
    if (new_tags == NULL)
        return false;

    new_tags[comment->tag_count] = tag;
    comment->tags = new_tags;
    comment->tag_count = next_count;
    return true;
}

static void
parser_add_doc_tag(Parser *parser, DocTagType type, const char *content)
{
    StructuredComment *comment;
    DocTag *tag;

    if (content == NULL)
        return;

    comment = parser_ensure_pending_doc_comment(parser);
    if (comment == NULL)
        return;

    tag = calloc(1, sizeof(DocTag));
    if (tag == NULL)
        return;

    tag->type = type;
    tag->content = parser_trimmed_copy(content);

    if (!parser_append_doc_tag(comment, tag)) {
        free(tag->content);
        free(tag);
        return;
    }
}

static void
parser_parse_doc_comment_line(Parser *parser, const char *line)
{
    char *trimmed;
    char *colon;

    if (line == NULL)
        return;

    trimmed = parser_trimmed_copy(line);
    if (trimmed == NULL || trimmed[0] == '\0') {
        free(trimmed);
        return;
    }

    if (trimmed[0] == '@') {
        char *name_start = trimmed + 1;
        char *cursor = name_start;
        DocTagType type;

        while (*cursor != '\0' && !isspace((unsigned char)*cursor) && *cursor != ':')
            cursor++;

        if (*cursor != '\0') {
            *cursor++ = '\0';
            while (*cursor != '\0' && (isspace((unsigned char)*cursor) || *cursor == ':'))
                cursor++;
        }

        if (parser_doc_tag_type_from_name(name_start, &type))
            parser_add_doc_tag(parser, type, cursor);

        free(trimmed);
        return;
    }

    if (trimmed[0] == '[') {
        char *close = strchr(trimmed, ']');
        DocTagType type;

        if (close != NULL) {
            char *content_start = close + 1;
            *close = '\0';
            while (*content_start != '\0' &&
                   (isspace((unsigned char)*content_start) || *content_start == ':'))
                content_start++;

            if (parser_doc_tag_type_from_name(trimmed + 1, &type))
                parser_add_doc_tag(parser, type, content_start);
        }

        free(trimmed);
        return;
    }

    colon = strchr(trimmed, ':');
    if (colon != NULL) {
        DocTagType type;
        *colon = '\0';
        if (parser_doc_tag_type_from_name(trimmed, &type))
            parser_add_doc_tag(parser, type, colon + 1);
    }

    free(trimmed);
}

void
parser_collect_doc_comments(Parser *parser)
{
    while (parser_check(parser, TOKEN_DOC_COMMENT)) {
        Token doc = parser_advance(parser);
        parser_parse_doc_comment_line(parser, doc.text);
    }
}

void
parser_discard_pending_doc_comment(Parser *parser)
{
    if (parser == NULL)
        return;

    ast_destroy_structured_comment(parser->pending_doc_comment);
    parser->pending_doc_comment = NULL;
}

StructuredComment *
parser_take_pending_doc_comment(Parser *parser)
{
    StructuredComment *comment;

    if (parser == NULL)
        return NULL;

    comment = parser->pending_doc_comment;
    parser->pending_doc_comment = NULL;
    return comment;
}

bool
parser_attach_pending_doc_comment(Parser *parser, ASTNode *node)
{
    if (parser == NULL || node == NULL || parser->pending_doc_comment == NULL)
        return false;

    switch (node->type) {
        case AST_FUNC_DECL:
            if (parser->last_func_decl_async) {
                node->data.async_func_decl.doc_comment = parser->pending_doc_comment;
            } else {
                node->data.func_decl.doc_comment = parser->pending_doc_comment;
            }
            parser->pending_doc_comment = NULL;
            return true;
        case AST_CLASS_DECL:
            node->data.class_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_ABILITY_DECL:
            node->data.ability_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_ROLE_DECL:
            node->data.role_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_PARTY_DECL:
            node->data.party_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_ROSTER_DECL:
            node->data.roster_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_WORLD_DECL:
            node->data.world_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_INTENT_DECL:
            node->data.intent_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_RELATION_DECL:
            node->data.relation_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_EFFECT_DECL:
            node->data.effect_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_ZONE_DECL:
            node->data.zone_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        default:
            return false;
    }
}
