#ifndef PERGYRA_FMT_LAYOUT_H
#define PERGYRA_FMT_LAYOUT_H

#include <stdbool.h>
#include <stdio.h>

#include "../lexer/lexer.h"

typedef struct {
    FILE *out;
    int indent;
    bool at_line_start;
    bool needs_blank_before_block;
} FmtCtx;

bool fmt_token_is_binary_operator(PgyTokenType type);
bool fmt_token_needs_space(Token prev, Token current);
bool fmt_token_starts_toplevel_decl(PgyTokenType type);
bool fmt_token_is_case_label(PgyTokenType type);
void fmt_indent(FmtCtx *ctx);
void fmt_newline(FmtCtx *ctx);

#endif
