/*
 * Copyright (c) 2025 Pergyra Language Project
 * Intent step parser owner.
 * BSD 3-Clause License
 */

#ifndef PERGYRA_PARSER_INTENT_STEP_H
#define PERGYRA_PARSER_INTENT_STEP_H

static void
parse_intent_step_transfer_clause(Parser *parser, ASTNode *step, bool shorthand_move)
{
    Token from_alias;
    Token to_alias;

    if (step->data.intent_step.transfer_from_alias != NULL
        || step->data.intent_step.transfer_to_alias != NULL) {
        parser_error(parser, "Duplicate '%s' clause in intent step",
            shorthand_move ? "move" : "transfer");
        return;
    }

    if (!shorthand_move) {
        parser_consume(parser, TOKEN_COLON, "Expected ':' after 'transfer'");
    }

    from_alias = consume_binding_name_token(
        parser,
        shorthand_move
            ? "Expected source zone alias after 'move'"
            : "Expected source zone alias after 'transfer:'");

    if (shorthand_move) {
        if (!parser_intent_match_keyword(parser, "to")) {
            parser_error(parser, "Expected 'to' after transfer source in move clause");
            return;
        }
    } else if (!parser_match(parser, TOKEN_ARROW)) {
        parser_consume(parser, TOKEN_MINUS, "Expected '->' in transfer clause");
        parser_consume(parser, TOKEN_GREATER, "Expected '->' in transfer clause");
    }

    to_alias = consume_binding_name_token(
        parser,
        shorthand_move
            ? "Expected target zone alias after 'to'"
            : "Expected target zone alias after '->'");
    free(step->data.intent_step.transfer_from_alias);
    free(step->data.intent_step.transfer_to_alias);
    step->data.intent_step.transfer_from_alias = pergyra_strdup(from_alias.text);
    step->data.intent_step.transfer_to_alias = pergyra_strdup(to_alias.text);
    parser_consume(
        parser, TOKEN_SEMICOLON,
        shorthand_move
            ? "Expected ';' after move clause"
            : "Expected ';' after step transfer clause");
}

static ASTNode *
parse_intent_step(Parser *parser)
{
    Token name_tok = consume_decl_name_token(parser, "Expected step name");
    ASTNode *step = ast_create_intent_step(name_tok.text);
    step->line = name_tok.line;
    step->column = name_tok.column;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after step name");
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_WHERE)) {
            if (step->data.intent_step.where_type != NULL) {
                parser_error(parser, "Duplicate 'where' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'where'");
            ast_destroy(step->data.intent_step.where_type);
            step->data.intent_step.where_type = parse_type(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step where clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "who")) {
            if (step->data.intent_step.who_count > 0) {
                parser_error(parser, "Duplicate 'who' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'who'");
            parse_intent_name_list(parser,
                &step->data.intent_step.who_names,
                &step->data.intent_step.who_count,
                "Expected involves alias after 'who:'");
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step who clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "using")) {
            if (step->data.intent_step.using_expr != NULL) {
                parser_error(parser, "Duplicate 'using' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'using'");
            ast_destroy(step->data.intent_step.using_expr);
            step->data.intent_step.using_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step using clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "intent")) {
            if (step->data.intent_step.intent_expr != NULL) {
                parser_error(parser, "Duplicate 'intent' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'intent'");
            ast_destroy(step->data.intent_step.intent_expr);
            step->data.intent_step.intent_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step intent clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "transfer")) {
            parse_intent_step_transfer_clause(parser, step, false);
            continue;
        }

        if (parser_intent_match_keyword(parser, "move")) {
            parse_intent_step_transfer_clause(parser, step, true);
            continue;
        }

        if (parser_intent_match_keyword(parser, "on")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'on'");
            intent_append_node(&step->data.intent_step.on_exprs,
                &step->data.intent_step.on_expr_count,
                parser_parse_expression(parser));
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step on clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "compensate")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'compensate'");
            intent_append_node(&step->data.intent_step.compensate_exprs,
                &step->data.intent_step.compensate_expr_count,
                parser_parse_expression(parser));
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step compensate clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "pre")) {
            if (step->data.intent_step.pre_expr != NULL) {
                parser_error(parser, "Duplicate 'pre' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'pre'");
            ast_destroy(step->data.intent_step.pre_expr);
            step->data.intent_step.pre_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step pre clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "guard")) {
            if (step->data.intent_step.guard_expr != NULL) {
                parser_error(parser, "Duplicate 'guard' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'guard'");
            ast_destroy(step->data.intent_step.guard_expr);
            step->data.intent_step.guard_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step guard clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "post")) {
            if (step->data.intent_step.post_expr != NULL) {
                parser_error(parser, "Duplicate 'post' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'post'");
            ast_destroy(step->data.intent_step.post_expr);
            step->data.intent_step.post_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step post clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "invariant")) {
            if (step->data.intent_step.invariant_expr != NULL) {
                parser_error(parser, "Duplicate 'invariant' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'invariant'");
            ast_destroy(step->data.intent_step.invariant_expr);
            step->data.intent_step.invariant_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step invariant clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "requires")) {
            if (step->data.intent_step.required_ability_count > 0) {
                parser_error(parser, "Duplicate 'requires' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'requires'");
            do {
                ASTNode *ability = parse_type(parser);
                size_t next = step->data.intent_step.required_ability_count + 1;
                step->data.intent_step.required_abilities = realloc(
                    step->data.intent_step.required_abilities,
                    next * sizeof(ASTNode *));
                step->data.intent_step.required_abilities[next - 1] = ability;
                step->data.intent_step.required_ability_count = next;
            } while (parser_match(parser, TOKEN_COMMA));
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step requires clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "authorized")) {
            if (step->data.intent_step.authorized_by_count > 0) {
                parser_error(parser, "Duplicate 'authorized by' clause in intent step");
                return step;
            }
            if (!parser_intent_match_keyword(parser, "by")) {
                parser_error(parser,
                    "Expected 'by' after 'authorized' in intent step clause; use 'authorized by: <participant>'");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'authorized by'");
            parse_intent_name_list(parser,
                &step->data.intent_step.authorized_by,
                &step->data.intent_step.authorized_by_count,
                "Expected involves alias after 'authorized by:'");
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step authorization clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "causes")) {
            Token effect_name;
            if (step->data.intent_step.causes_effect != NULL) {
                parser_error(parser, "Duplicate 'causes' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'causes'");
            effect_name = consume_name_token(parser, "Expected effect name after 'causes:'");
            free(step->data.intent_step.causes_effect);
            step->data.intent_step.causes_effect = pergyra_strdup(effect_name.text);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step causes clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "within")) {
            parser_error(parser,
                "'within' is an action clause, not an intent step clause; "
                "use 'where: <Zone>;' on the step or omit it to reuse the matching action zone contract");
            return step;
        }

        if (parser_match(parser, TOKEN_WITH)) {
            parser_error(parser,
                "'with effects ...' is not a valid intent step clause; "
                "use 'causes: <Effect>;' on the step or declare effects on the matching action contract");
            return step;
        }

        if (parser_intent_match_keyword(parser, "intent")) {
            if (step->data.intent_step.intent_expr != NULL) {
                parser_error(parser, "Duplicate 'intent' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'intent'");
            ast_destroy(step->data.intent_step.intent_expr);
            step->data.intent_step.intent_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step intent clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "expect")) {
            if (step->data.intent_step.expect_expr != NULL) {
                parser_error(parser, "Duplicate 'expect' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'expect'");
            ast_destroy(step->data.intent_step.expect_expr);
            step->data.intent_step.expect_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step expect clause");
            continue;
        }

        parser_error(parser,
            "Unsupported intent step clause; expected one of: "
            "where:, who:, using:, intent:, transfer:, move, on:, compensate:, "
            "pre:, guard:, post:, invariant:, requires:, authorized by:, causes:, expect:. "
            "Action-only clauses such as 'within' and 'with effects' belong on the matching action contract.");
        return step;
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after step body");
    return step;
}

#endif /* PERGYRA_PARSER_INTENT_STEP_H */
