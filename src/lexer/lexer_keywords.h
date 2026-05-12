#ifndef PGY_LEXER_KEYWORDS_H
#define PGY_LEXER_KEYWORDS_H

#include <stddef.h>

#include "lexer.h"

PgyTokenType lexer_lookup_keyword(const char *text, size_t length);

#endif /* PGY_LEXER_KEYWORDS_H */
