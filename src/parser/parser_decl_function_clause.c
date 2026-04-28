#include "parser_internal.h"

typedef bool (*FunctionClauseParser)(Parser *parser, ASTNode *func,
                                     bool is_action);

static bool
parser_decl_is_action_only_function_clause(Parser *parser)
{
    if (parser == NULL || !parser_check(parser, TOKEN_IDENTIFIER)
        || parser->current_token.text == NULL) {
        return false;
    }

    return strcmp(parser->current_token.text, "requires") == 0
        || strcmp(parser->current_token.text, "within") == 0
        || strcmp(parser->current_token.text, "causes") == 0
        || strcmp(parser->current_token.text, "authorized") == 0;
}

static bool
parser_decl_is_function_clause_leader(Parser *parser)
{
    return parser != NULL
        && (parser_check(parser, TOKEN_WHERE)
            || parser_check(parser, TOKEN_WITH)
            || parser_decl_is_action_only_function_clause(parser));
}

static bool
parser_decl_report_invalid_function_clause(Parser *parser, bool is_action)
{
    if (parser == NULL)
        return false;

    if (!is_action && parser_decl_is_action_only_function_clause(parser)) {
        parser_error(parser,
                     "'%s' clause is only valid on 'action' declarations; "
                     "use 'action Name(...)' or remove the clause",
                     parser->current_token.text);
        return true;
    }

    if (parser_decl_is_function_clause_leader(parser)) {
        parser_error(parser,
                     "Invalid function/action clause; expected one of "
                     "'where', 'with effects', 'requires', 'within', "
                     "'causes', or 'authorized by'");
        return true;
    }

    return false;
}

static bool
parse_function_clause_where(Parser *parser, ASTNode *func, bool is_action)
{
    WhereClause *where_clause = NULL;
    (void)is_action;
    if (parser == NULL || func == NULL || !parser_check(parser, TOKEN_WHERE))
        return false;
    if (func->data.func_decl.where_clause != NULL)
        parser_error(parser, "Duplicate 'where' clause");
    where_clause = parse_where_clause(parser);
    if (func->data.func_decl.where_clause == NULL)
        func->data.func_decl.where_clause = where_clause;
    return true;
}

static bool
parse_function_clause_effects(Parser *parser, ASTNode *func, bool is_action)
{
    bool has_clause = false;
    uint32_t mask = 0;
    (void)is_action;
    if (parser == NULL || func == NULL || !parser_check(parser, TOKEN_WITH))
        return false;
    if (func->data.func_decl.has_effects_clause)
        parser_error(parser, "Duplicate 'with effects' clause");
    parse_optional_effect_clause(parser, &has_clause, &mask);
    if (!func->data.func_decl.has_effects_clause && has_clause) {
        func->data.func_decl.has_effects_clause = true;
        func->data.func_decl.declared_effects = mask;
    }
    return true;
}

static bool
parse_function_clause_requires(Parser *parser, ASTNode *func, bool is_action)
{
    if (parser == NULL || func == NULL || !is_action
        || !parser_decl_check_contextual_keyword(parser, "requires")) {
        return false;
    }
    if (func->data.func_decl.required_ability_count > 0)
        parser_error(parser, "Duplicate 'requires' clause");
    parser_advance(parser);
    if (parser_match(parser, TOKEN_COLON)) {
        parser_error(parser,
                     "Intent-style 'requires:' is not valid in function/action clauses; "
                     "use 'requires Ability[, Ability]'");
        return true;
    }
    do {
        ASTNode *ability = parse_type(parser);
        size_t next = func->data.func_decl.required_ability_count + 1;
        func->data.func_decl.required_abilities = realloc(
            func->data.func_decl.required_abilities,
            next * sizeof(ASTNode *));
        func->data.func_decl.required_abilities[next - 1] = ability;
        func->data.func_decl.required_ability_count = next;
    } while (parser_match(parser, TOKEN_COMMA));
    return true;
}

static bool
parse_function_clause_within(Parser *parser, ASTNode *func, bool is_action)
{
    Token zone;
    if (parser == NULL || func == NULL || !is_action
        || !parser_decl_check_contextual_keyword(parser, "within")) {
        return false;
    }
    if (func->data.func_decl.within_zone != NULL)
        parser_error(parser, "Duplicate 'within' clause");
    parser_advance(parser);
    if (parser_match(parser, TOKEN_COLON)) {
        parser_error(parser,
                     "Intent-style 'within:' is not valid in function/action clauses; "
                     "use 'within ZoneName'");
        return true;
    }
    zone = parser_consume(parser, TOKEN_IDENTIFIER,
        "Expected zone name after 'within'");
    if (func->data.func_decl.within_zone == NULL)
        func->data.func_decl.within_zone = pergyra_strdup(zone.text);
    return true;
}

static bool
parse_function_clause_causes(Parser *parser, ASTNode *func, bool is_action)
{
    Token effect;
    if (parser == NULL || func == NULL || !is_action
        || !parser_decl_check_contextual_keyword(parser, "causes")) {
        return false;
    }
    if (func->data.func_decl.causes_effect != NULL)
        parser_error(parser, "Duplicate 'causes' clause");
    parser_advance(parser);
    if (parser_match(parser, TOKEN_COLON)) {
        parser_error(parser,
                     "Intent-style 'causes:' is not valid in function/action clauses; "
                     "use 'causes EffectName'");
        return true;
    }
    effect = parser_consume(parser, TOKEN_IDENTIFIER,
        "Expected effect name after 'causes'");
    if (func->data.func_decl.causes_effect == NULL)
        func->data.func_decl.causes_effect = pergyra_strdup(effect.text);
    return true;
}

static bool
parse_function_clause_authorized_by(Parser *parser, ASTNode *func,
                                    bool is_action)
{
    if (parser == NULL || func == NULL || !is_action
        || !parser_decl_check_contextual_keyword(parser, "authorized")) {
        return false;
    }
    if (func->data.func_decl.authorized_by_count > 0)
        parser_error(parser, "Duplicate 'authorized by' clause");
    parser_advance(parser);
    if (!parser_decl_match_contextual_keyword(parser, "by")) {
        parser_error(parser,
                     "Expected 'by' after 'authorized' in function/action clause; use 'authorized by <subject>'");
        return true;
    }
    if (parser_match(parser, TOKEN_COLON)) {
        parser_error(parser,
                     "Intent-style 'authorized by:' is not valid in function/action clauses; "
                     "use 'authorized by <subject>'");
        return true;
    }
    do {
        Token participant = parser_consume(parser, TOKEN_IDENTIFIER,
            "Expected subject name after 'authorized by'");
        size_t next = func->data.func_decl.authorized_by_count + 1;
        func->data.func_decl.authorized_by = realloc(
            func->data.func_decl.authorized_by,
            next * sizeof(char *));
        func->data.func_decl.authorized_by[next - 1] =
            pergyra_strdup(participant.text);
        func->data.func_decl.authorized_by_count = next;
    } while (parser_match(parser, TOKEN_COMMA));
    return true;
}

bool
parser_decl_parse_next_function_clause(Parser *parser, ASTNode *func,
                                       bool is_action, bool *matched_out)
{
    static const FunctionClauseParser clause_parsers[] = {
        parse_function_clause_where,
        parse_function_clause_effects,
        parse_function_clause_requires,
        parse_function_clause_within,
        parse_function_clause_causes,
        parse_function_clause_authorized_by,
    };

    if (matched_out != NULL)
        *matched_out = false;

    for (size_t i = 0; i < sizeof(clause_parsers) / sizeof(clause_parsers[0]); i++) {
        if (!clause_parsers[i](parser, func, is_action))
            continue;
        if (matched_out != NULL)
            *matched_out = true;
        return true;
    }

    if (parser_has_error(parser))
        return false;

    if (parser_decl_report_invalid_function_clause(parser, is_action))
        return false;

    return true;
}
