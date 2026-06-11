#include "parser_domain_internal.h"


/* =================================================================
 * Roster/World system parsing functions
 * ================================================================= */

/*
 * roster CombatSystem {
 *     party slot team1: DungeonTeam
 *     party slot team2: DungeonTeam
 *     shared rules: CombatRules
 *     func ScheduleMatches() -> Void { ... }
 * }
 */
/*
 * world GameWorld {
 *     roster combat: CombatSystem
 *     roster economy: EconomySystem
 *     shared tick: Int = 0
 *     func Update() -> Void { ... }
 * }
 */
bool
parser_match_identifier_keyword(Parser *parser, const char *keyword)
{
    if (!parser_check(parser, TOKEN_IDENTIFIER)
        || parser->current_token.text == NULL
        || keyword == NULL) {
        return false;
    }

    if (strcmp(parser->current_token.text, keyword) != 0)
        return false;

    parser_advance(parser);
    return true;
}

bool
parser_match_identifier_keyword_on_line(Parser *parser, const char *keyword,
                                        unsigned line)
{
    if (parser == NULL || parser->current_token.line != line)
        return false;

    return parser_match_identifier_keyword(parser, keyword);
}

static bool
parser_match_domain_slot_kind(Parser *parser, bool *is_subject, bool *is_vessel,
                              bool *is_tobject)
{
    if (parser_match(parser, TOKEN_SUBJECT)) {
        if (is_subject != NULL)
            *is_subject = true;
        if (is_vessel != NULL)
            *is_vessel = false;
        if (is_tobject != NULL)
            *is_tobject = false;
        return true;
    }

    if (parser_match(parser, TOKEN_VESSEL)) {
        if (is_subject != NULL)
            *is_subject = false;
        if (is_vessel != NULL)
            *is_vessel = true;
        if (is_tobject != NULL)
            *is_tobject = false;
        return true;
    }

    if (parser_match(parser, TOKEN_TOBJECT)) {
        if (is_subject != NULL)
            *is_subject = false;
        if (is_vessel != NULL)
            *is_vessel = false;
        if (is_tobject != NULL)
            *is_tobject = true;
        return true;
    }

    if (parser_match(parser, TOKEN_OBJECT)) {
        if (is_subject != NULL)
            *is_subject = false;
        if (is_vessel != NULL)
            *is_vessel = false;
        if (is_tobject != NULL)
            *is_tobject = false;
        return true;
    }

    return false;
}

ASTNode *
parse_domain_slot_entry(Parser *parser, const char *owner_name)
{
    bool is_subject = false;
    bool is_vessel = false;
    bool is_tobject = false;
    if (!parser_match_domain_slot_kind(parser, &is_subject, &is_vessel, &is_tobject))
        return NULL;

    parser_consume(parser, TOKEN_SLOT,
        is_subject
            ? "Expected 'slot' after 'subject' in domain body"
            : (is_vessel
                ? "Expected 'slot' after 'vessel' in domain body"
            : (is_tobject
                ? "Expected 'slot' after 'tobject' in domain body"
                : "Expected 'slot' after 'object' in domain body")));
    Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
        "Expected slot name");
    parser_consume(parser, TOKEN_COLON,
        "Expected ':' after domain slot name");
    ASTNode *slot_type = parse_type(parser);

    ASTNode *slot = ast_create_domain_slot(slot_name.text, is_subject);
    slot->data.domain_slot.is_vessel = is_vessel;
    slot->data.domain_slot.is_tobject = is_tobject;
    slot->data.domain_slot.type = slot_type;
    if (parser_match(parser, TOKEN_ASSIGN))
        slot->data.domain_slot.initializer = parser_parse_expression(parser);
    slot->line = slot_name.line;
    slot->column = slot_name.column;

    (void)owner_name;
    return slot;
}

void
append_domain_slot(ASTNode ***slots, size_t *slot_count,
                   size_t *slot_capacity, ASTNode *slot)
{
    ASTNode **grown;
    size_t next_capacity;

    if (slots == NULL || slot_count == NULL || slot_capacity == NULL
        || slot == NULL)
        return;

    if (*slot_count >= *slot_capacity) {
        next_capacity = *slot_capacity == 0 ? 4 : *slot_capacity * 2;
        if (next_capacity <= *slot_count
            || next_capacity > (size_t)-1 / sizeof(ASTNode *))
            return;
        grown = realloc(*slots, next_capacity * sizeof(ASTNode *));
        if (grown == NULL)
            return;
        *slots = grown;
        *slot_capacity = next_capacity;
    }

    (*slots)[*slot_count] = slot;
    *slot_count += 1;
}

void
append_child_node(ASTNode ***nodes, size_t *count, size_t *capacity,
                  ASTNode *node)
{
    ASTNode **grown;
    size_t next_capacity;

    if (nodes == NULL || count == NULL || capacity == NULL || node == NULL)
        return;

    if (*count >= *capacity) {
        next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        if (next_capacity <= *count
            || next_capacity > (size_t)-1 / sizeof(ASTNode *))
            return;
        grown = realloc(*nodes, next_capacity * sizeof(ASTNode *));
        if (grown == NULL)
            return;
        *nodes = grown;
        *capacity = next_capacity;
    }

    (*nodes)[*count] = node;
    *count += 1;
}

char *
parse_optional_zone_participant_name(Parser *parser)
{
    Token participant_slot;

    if (!parser_match_identifier_keyword(parser, "by"))
        return NULL;

    participant_slot = consume_name_token(parser,
        "Expected subject slot name after 'by'");
    return pergyra_strdup(participant_slot.text);
}

ASTNode* parse_party_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected party name");
    ASTNode* party = ast_create_party_declaration(name.text);
    party->data.party_decl.doc_comment = parser_take_pending_doc_comment(parser);
    party->line = name.line;
    party->column = name.column;

    /* Optional generic params */
    party->data.party_decl.generic_params = parse_generic_params(parser);

    /* Optional extends */
    if (parser_match(parser, TOKEN_EXTENDS)) {
        party->data.party_decl.extends = parse_type(parser);
    }

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after party header");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);
        bool is_dyn = parser_match(parser, TOKEN_DYN);

        if (is_dyn || parser_match(parser, TOKEN_ROLE)) {
            if (is_dyn) {
                parser_consume(parser, TOKEN_ROLE,
                    "Expected 'role' after 'dyn'");
            }
            /* role slot name: AbilityType & AbilityType */
            parser_consume(parser, TOKEN_SLOT,
                "Expected 'slot' after 'role' in party");
            Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected slot name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after role slot name");

            ASTNode* rs = ast_create_role_slot(slot_name.text);
            rs->line = slot_name.line;
            rs->column = slot_name.column;
            rs->data.role_slot.is_dynamic = is_dyn;

            /* Parse ability intersection types separated by `&`. */
            do {
                ASTNode* ability_type = parse_type(parser);
                append_child_node(&rs->data.role_slot.required_abilities,
                    &rs->data.role_slot.ability_count,
                    &rs->data.role_slot.ability_capacity, ability_type);
            } while (parser_match(parser, TOKEN_AMP));

            if (parser_check(parser, TOKEN_AND)) {
                parser_error(parser,
                    "Role slot ability intersection uses '&', not '&&'.\n"
                    "Reason: '&&' is a boolean expression operator and must not be an ability-contract alias.\n"
                    "Fix: write 'role slot tank: Damageable & Guardable'.");
            }

            if (parser_check(parser, TOKEN_PATTERN_OR)) {
                parser_error(parser,
                    "Role slot ability union '|' is reserved but not implemented.\n"
                    "Reason: role-slot OR contracts need stable bind-time ambiguity diagnostics before they can be beta-stable.\n"
                    "Fix: use an explicit top-level intersection with '&' or split the role slot.");
            }

            append_child_node(&party->data.party_decl.role_slots,
                &party->data.party_decl.role_count,
                &party->data.party_decl.role_capacity, rs);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_SHARED)) {
            /* shared field_name: Type = initializer */
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected field name after 'shared'");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after shared field name");
            ASTNode* field_type = parse_type(parser);

            ASTNode* shared = ast_create_party_shared(field_name.text);
            shared->data.party_shared.type = field_type;
            shared->line = field_name.line;
            shared->column = field_name.column;

            if (parser_match(parser, TOKEN_ASSIGN)) {
                shared->data.party_shared.initializer =
                    parser_parse_expression(parser);
            }

            append_child_node(&party->data.party_decl.shared_fields,
                &party->data.party_decl.shared_count,
                &party->data.party_decl.shared_capacity, shared);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            /* Party method */
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));

            append_child_node(&party->data.party_decl.methods,
                &party->data.party_decl.method_count,
                &party->data.party_decl.method_capacity, method);

        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'role slot', 'shared', or 'func' in party body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after party body");
    return party;
}

/* =================================================================
 * Role/Ability system parsing functions
 * ================================================================= */

/*
 * ability Damageable {
 *     fields health: Int
 *     func TakeDamage(amount: Int) -> Void
 *     func GetHealth() -> Int { return self.health; }
 * }
 */
ASTNode* parse_ability_declaration(Parser* parser, bool is_innate) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected ability name");
    ASTNode* ability = ast_create_ability_declaration(name.text);
    ability->data.ability_decl.is_innate = is_innate;
    ability->data.ability_decl.doc_comment = parser_take_pending_doc_comment(parser);
    ability->line = name.line;
    ability->column = name.column;
    ability->data.ability_decl.generic_params = parse_generic_params(parser);
    ability->data.ability_decl.where_clause = parse_where_clause(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after ability name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);
        if (parser_match_identifier_keyword(parser, "fields")) {
            /* fields field_name: Type */
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected field name after 'fields'");
            parser_consume(parser, TOKEN_COLON, "Expected ':' after fields field name");
            ASTNode* field_type = parse_type(parser);

            ASTNode* req = ast_create_require_field(field_name.text);
            req->data.require_field.type = field_type;
            req->line = field_name.line;
            req->column = field_name.column;

            append_child_node(&ability->data.ability_decl.require_fields,
                &ability->data.ability_decl.require_count,
                &ability->data.ability_decl.require_capacity, req);

            /* Optional semicolon */
            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            /* Method declaration (may have body or be abstract) */
            bool saved_abstract_ctx = parser->in_abstract_method_context;
            parser->in_abstract_method_context = true;
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));
            parser->in_abstract_method_context = saved_abstract_ctx;

            append_child_node(&ability->data.ability_decl.methods,
                &ability->data.ability_decl.method_count,
                &ability->data.ability_decl.method_capacity, method);

        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser, "Expected 'fields' or 'func' in ability body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after ability body");
    return ability;
}

/*
 * role PlayerDamageable for Player {
 *     include role BuffableRole<Int>
 *     impl ability Damageable {
 *         func TakeDamage(amount: Int) -> Void { ... }
 *     }
 *     override func GetHealth() -> Int { super.GetHealth() + bonus; }
 * }
 */
ASTNode* parse_role_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected role name");
    ASTNode* role = ast_create_role_declaration(name.text);
    role->data.role_decl.doc_comment = parser_take_pending_doc_comment(parser);
    role->line = name.line;
    role->column = name.column;

    /* Optional generic params */
    role->data.role_decl.generic_params = parse_generic_params(parser);

    /* 'for' TargetType (reuse TOKEN_FOR) */
    if (parser_match(parser, TOKEN_FOR)) {
        role->data.role_decl.for_type = parse_type(parser);
    }

    /* Optional where clause */
    role->data.role_decl.where_clause = parse_where_clause(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after role header");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);
        if (parser_match(parser, TOKEN_INCLUDE)) {
            /* include role RoleName<T> */
            parser_match(parser, TOKEN_ROLE); /* optional 'role' keyword */
            Token role_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected role name after 'include'");
            ASTNode* inc = ast_create_include_statement(role_name.text);
            inc->line = role_name.line;
            inc->column = role_name.column;
            /* Optional type args */
            inc->data.include_stmt.type_args = parse_type_arguments(parser);

            append_child_node(&role->data.role_decl.includes,
                &role->data.role_decl.include_count,
                &role->data.role_decl.include_capacity, inc);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_IMPL)) {
            /* impl ability AbilityName { ... } */
            parser_match(parser, TOKEN_ABILITY); /* optional 'ability' keyword */
            ASTNode *ability_ref = parse_type(parser);
            ASTNode* impl = ast_create_impl_ability(ability_ref);
            impl->line = ability_ref != NULL ? ability_ref->line : name.line;
            impl->column = ability_ref != NULL ? ability_ref->column : name.column;

            parser_consume(parser, TOKEN_LBRACE,
                "Expected '{' after impl ability name");

            while (!parser_check(parser, TOKEN_RBRACE)
                   && !parser_is_at_end(parser)) {
                parser_collect_doc_comments(parser);
                if (parser_match(parser, TOKEN_FUNC)) {
                    ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));
                    append_child_node(&impl->data.impl_ability.methods,
                        &impl->data.impl_ability.method_count,
                        &impl->data.impl_ability.method_capacity, method);
                } else {
                    parser_discard_pending_doc_comment(parser);
                    parser_error(parser,
                        "Expected 'func' in impl ability body");
                    parser_advance(parser);
                }
            }
            parser_consume(parser, TOKEN_RBRACE,
                "Expected '}' after impl ability body");

            append_child_node(&role->data.role_decl.impl_abilities,
                &role->data.role_decl.impl_count,
                &role->data.role_decl.impl_capacity, impl);

        } else if (parser_match(parser, TOKEN_OVERRIDE)) {
            /* override func FuncName(...) { ... } */
            parser_consume(parser, TOKEN_FUNC,
                "Expected 'func' after 'override'");
            ASTNode* func = parser_finalize_statement(parser, parse_function_declaration(parser));
            ASTNode* ovr = ast_create_override_func(func);
            ovr->line = func->line;
            ovr->column = func->column;

            /* Check if body contains 'super' calls; simple heuristic. */
            ovr->data.override_func.calls_super = false;

            /* Add as an impl with special name "__override__" */
            append_child_node(&role->data.role_decl.impl_abilities,
                &role->data.role_decl.impl_count,
                &role->data.role_decl.impl_capacity, ovr);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            /* Direct method in role (not in impl block) */
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));

            /* Wrap as impl with no ability name (role's own method) */
            ASTNode* impl = ast_create_impl_ability(NULL);
            impl->data.impl_ability.method_count = 1;
            impl->data.impl_ability.method_capacity = 1;
            impl->data.impl_ability.methods = calloc(1, sizeof(ASTNode*));
            impl->data.impl_ability.methods[0] = method;

            append_child_node(&role->data.role_decl.impl_abilities,
                &role->data.role_decl.impl_count,
                &role->data.role_decl.impl_capacity, impl);

        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'include', 'impl', 'override', or 'func' in role body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after role body");
    return role;
}

/* =================================================================
 * Event system parsing functions
 * ================================================================= */

// Event declaration parsing: event OnClick(sender: Object, args: EventArgs);
