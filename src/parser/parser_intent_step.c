#include "parser_internal.h"

static bool
parse_intent_outcome_branch(Parser *parser,
                            ASTIntentOutcomeBranchData *branch,
                            const char *clause_name)
{
    Token variant;
    Token payload;

    if (branch == NULL)
        return false;
    if (branch->variant_name != NULL) {
        parser_error(parser, "Duplicate '%s' clause in intent step",
                     clause_name);
        return false;
    }

    parser_consume(parser, TOKEN_COLON,
                   strcmp(clause_name, "success") == 0
                       ? "Expected ':' after step 'success'"
                       : "Expected ':' after step 'failure'");
    variant = consume_name_token(
        parser,
        strcmp(clause_name, "success") == 0
            ? "Expected exact action-result variant after step 'success:'"
            : "Expected exact action-result variant after step 'failure:'");
    parser_consume(parser, TOKEN_LPAREN,
                   "Expected '(' after action-result variant");
    payload = consume_binding_name_token(
        parser, "Expected payload binding in action-result variant pattern");
    parser_consume(parser, TOKEN_RPAREN,
                   "Expected ')' after action-result payload binding");
    parser_consume(parser, TOKEN_SEMICOLON,
                   "Expected ';' after intent step outcome branch");

    branch->variant_name = pergyra_strdup(variant.text);
    branch->payload_name = pergyra_strdup(payload.text);
    if (branch->variant_name == NULL || branch->payload_name == NULL) {
        parser_error(parser,
                     "Out of memory while recording intent step outcome branch");
        return false;
    }
    return true;
}

static bool
parser_intent_step_append_required_ability(Parser *parser, ASTNode *step,
                                           ASTNode *ability)
{
    ASTNode **grown;
    size_t next_capacity;

    if (parser == NULL || step == NULL || ability == NULL)
        return false;

    if (step->data.intent_step.required_ability_count
        >= step->data.intent_step.required_ability_capacity) {
        next_capacity = step->data.intent_step.required_ability_capacity == 0
            ? 4 : step->data.intent_step.required_ability_capacity * 2;
        if (next_capacity <= step->data.intent_step.required_ability_count
            || next_capacity > (size_t)-1 / sizeof(ASTNode *)) {
            parser_error(parser, "Out of memory while parsing intent step requires");
            return false;
        }
        grown = realloc(step->data.intent_step.required_abilities,
                        next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while parsing intent step requires");
            return false;
        }
        step->data.intent_step.required_abilities = grown;
        step->data.intent_step.required_ability_capacity = next_capacity;
    }

    step->data.intent_step.required_abilities[
        step->data.intent_step.required_ability_count++] = ability;
    return true;
}

static void
parse_intent_step_transfer_clause(Parser *parser, ASTNode *step,
                                  bool shorthand_move)
{
    Token from_alias;
    Token to_alias;

    if (step->data.intent_step.transfer_from_alias != NULL
        || step->data.intent_step.transfer_to_alias != NULL) {
        parser_error(parser, "Duplicate '%s' clause in intent step",
            shorthand_move ? "move" : "transfer");
        return;
    }

    if (!shorthand_move)
        parser_consume(parser, TOKEN_COLON, "Expected ':' after 'transfer'");

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

ASTNode *
parse_intent_step(Parser *parser)
{
    Token name_tok = consume_decl_name_token(parser, "Expected step name");
    ASTNode *step = ast_create_intent_step(name_tok.text);
    step->line = name_tok.line;
    step->column = name_tok.column;

    if (parser_intent_match_keyword(parser, "after")) {
        Token predecessor = consume_decl_name_token(
            parser, "Expected predecessor step name after 'after'");
        step->data.intent_step.predecessor_step_name =
            pergyra_strdup(predecessor.text);
        if (step->data.intent_step.predecessor_step_name == NULL) {
            parser_error(parser,
                         "Out of memory while recording intent step predecessor");
            return step;
        }
    }

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
                &step->data.intent_step.who_capacity,
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
            ASTNode *expr;
            if (parser_check_binding_name_token(parser)
                && parser_peek_next(parser).type == TOKEN_COLON) {
                Token binding;
                if (ast_intent_step_outcome_binding_name(step) != NULL) {
                    parser_error(parser,
                        "Duplicate outcome binding in intent step; only one 'on <name>:' clause may bind an action result");
                    return step;
                }
                binding = consume_binding_name_token(
                    parser, "Expected outcome binding name after 'on'");
                if (binding.text == NULL || binding.text[0] == '\0'
                    || binding.length == 0) {
                    parser_error(parser,
                        "Intent step outcome binding name cannot be empty");
                    return step;
                }
                if (!ast_intent_step_set_outcome_binding_copy(
                        step, binding.text, binding.length,
                        binding.line, binding.column)) {
                    parser_error(parser,
                        "Out of memory while recording intent step outcome binding");
                    return step;
                }
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'on'");
            expr = parser_parse_expression(parser);
            intent_append_node(&step->data.intent_step.on_exprs,
                &step->data.intent_step.on_expr_count,
                &step->data.intent_step.on_expr_capacity, expr);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step on clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "compensate")) {
            ASTNode *expr;
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'compensate'");
            expr = parser_parse_expression(parser);
            intent_append_node(&step->data.intent_step.compensate_exprs,
                &step->data.intent_step.compensate_expr_count,
                &step->data.intent_step.compensate_expr_capacity, expr);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step compensate clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "success")) {
            if (!parse_intent_outcome_branch(
                    parser, &step->data.intent_step.success_branch,
                    "success")) {
                return step;
            }
            continue;
        }

        if (parser_intent_match_keyword(parser, "failure")) {
            if (!parse_intent_outcome_branch(
                    parser, &step->data.intent_step.failure_branch,
                    "failure")) {
                return step;
            }
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
            parser_consume(parser, TOKEN_SEMICOLON,
                           "Expected ';' after step invariant clause");
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
                if (!parser_intent_step_append_required_ability(parser, step, ability)) {
                    ast_destroy(ability);
                    break;
                }
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
                &step->data.intent_step.authorized_by_capacity,
                "Expected involves alias after 'authorized by:'");
            parser_consume(parser, TOKEN_SEMICOLON,
                           "Expected ';' after step authorization clause");
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
            "success:, failure:, pre:, guard:, post:, invariant:, requires:, authorized by:, causes:, expect:. "
            "Action-only clauses such as 'within' and 'with effects' belong on the matching action contract.");
        return step;
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after step body");
    return step;
}
