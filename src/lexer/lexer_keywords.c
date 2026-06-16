#include "lexer_keywords.h"

#include <string.h>

typedef struct {
    const char *keyword;
    PgyTokenType type;
} KeywordEntry;

static const KeywordEntry kKeywords[] = {
    {"ability", TOKEN_ABILITY},
    {"as", TOKEN_AS},
    {"async", TOKEN_ASYNC},
    {"await", TOKEN_AWAIT},
    {"bind", TOKEN_BIND},
    {"break", TOKEN_BREAK},
    {"case", TOKEN_CASE},
    {"channel", TOKEN_CHANNEL},
    {"class", TOKEN_CLASS},
    {"collapse", TOKEN_COLLAPSE},
    {"continue", TOKEN_CONTINUE},
    {"default", TOKEN_DEFAULT},
    {"defer", TOKEN_DEFER},
    {"dyn", TOKEN_DYN},
    {"effect", TOKEN_EFFECT},
    {"else", TOKEN_ELSE},
    {"enum", TOKEN_ENUM},
    {"event", TOKEN_EVENT},
    {"export", TOKEN_EXPORT},
    {"extends", TOKEN_EXTENDS},
    {"extern", TOKEN_EXTERN},
    {"false", TOKEN_FALSE},
    {"for", TOKEN_FOR},
    {"func", TOKEN_FUNC},
    {"if", TOKEN_IF},
    {"impl", TOKEN_IMPL},
    {"import", TOKEN_IMPORT},
    {"in", TOKEN_IN},
    {"include", TOKEN_INCLUDE},
    {"innate", TOKEN_INNATE},
    {"intent", TOKEN_INTENT},
    {"let", TOKEN_LET},
    {"local", TOKEN_LOCAL},
    {"match", TOKEN_MATCH},
    {"namespace", TOKEN_NAMESPACE},
    {"nondeterministic", TOKEN_NONDETERMINISTIC},
    {"object", TOKEN_OBJECT},
    {"override", TOKEN_OVERRIDE},
    {"own", TOKEN_OWN},
    {"parallel", TOKEN_PARALLEL},
    {"party", TOKEN_PARTY},
    {"private", TOKEN_PRIVATE},
    {"public", TOKEN_PUBLIC},
    {"ref", TOKEN_REF},
    {"reflect", TOKEN_REFLECT},
    {"relation", TOKEN_RELATION},
    {"remote", TOKEN_REMOTE},
    {"return", TOKEN_RETURN},
    {"role", TOKEN_ROLE},
    {"roster", TOKEN_ROSTER},
    {"secure", TOKEN_SECURE},
    {"select", TOKEN_SELECT},
    {"shared", TOKEN_SHARED},
    {"slot", TOKEN_SLOT},
    {"spawn", TOKEN_SPAWN},
    {"struct", TOKEN_STRUCT},
    {"subject", TOKEN_SUBJECT},
    {"tobject", TOKEN_TOBJECT},
    {"true", TOKEN_TRUE},
    {"type", PGY_TOKEN_TYPE},
    {"unsafe", TOKEN_UNSAFE},
    {"use", TOKEN_USE},
    {"vessel", TOKEN_VESSEL},
    {"where", TOKEN_WHERE},
    {"while", TOKEN_WHILE},
    {"with", TOKEN_WITH},
    {"world", TOKEN_WORLD},
    {"zone", TOKEN_ZONE},
};

static int
keyword_compare_slice(const char *text, size_t length, const char *keyword)
{
    size_t keyword_len = strlen(keyword);
    size_t limit = length < keyword_len ? length : keyword_len;
    int cmp = strncmp(text, keyword, limit);

    if (cmp != 0)
        return cmp;
    if (length < keyword_len)
        return -1;
    if (length > keyword_len)
        return 1;
    return 0;
}

PgyTokenType
lexer_lookup_keyword(const char *text, size_t length)
{
    size_t lo = 0;
    size_t hi = sizeof(kKeywords) / sizeof(kKeywords[0]);

    if (text == NULL)
        return TOKEN_IDENTIFIER;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = keyword_compare_slice(text, length, kKeywords[mid].keyword);
        if (cmp == 0)
            return kKeywords[mid].type;
        if (cmp < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return TOKEN_IDENTIFIER;
}
