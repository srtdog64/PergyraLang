/*
 * Copyright (c) 2025 Pergyra Language Project
 * Async parsing support for Pergyra parser
 * BSD Style + C# naming conventions
 */

#include "parser_internal.h"
#include <stdint.h>

static bool
parser_append_async_param(Parser *parser, ASTNode *func, FuncParam *param)
{
    FuncParam **grown;

    if (parser == NULL || func == NULL || param == NULL)
        return false;

    if (func->data.async_func_decl.param_count == func->data.async_func_decl.param_capacity) {
        size_t next_capacity = func->data.async_func_decl.param_capacity == 0
            ? 4
            : func->data.async_func_decl.param_capacity * 2;
        if (next_capacity < func->data.async_func_decl.param_capacity
            || next_capacity > SIZE_MAX / sizeof(FuncParam *)) {
            parser_error(parser, "Out of memory while parsing async function parameters");
            return false;
        }
        grown = realloc(func->data.async_func_decl.params, next_capacity * sizeof(FuncParam *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while parsing async function parameters");
            return false;
        }
        func->data.async_func_decl.params = grown;
        func->data.async_func_decl.param_capacity = next_capacity;
    }

    func->data.async_func_decl.params[func->data.async_func_decl.param_count++] = param;
    return true;
}

static bool
parser_append_async_node(Parser *parser, ASTNode ***nodes, size_t *count, size_t *capacity,
                         ASTNode *node, const char *error_message)
{
    ASTNode **grown;

    if (parser == NULL || nodes == NULL || count == NULL || capacity == NULL || node == NULL)
        return false;

    if (*count == *capacity) {
        size_t next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        if (next_capacity < *capacity
            || next_capacity > SIZE_MAX / sizeof(ASTNode *)) {
            parser_error(parser, error_message);
            return false;
        }
        grown = realloc(*nodes, next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            parser_error(parser, error_message);
            return false;
        }
        *nodes = grown;
        *capacity = next_capacity;
    }

    (*nodes)[(*count)++] = node;
    return true;
}

// Parse async function declaration
ASTNode* parser_parse_async_function(Parser* parser)
{
    // 'async' keyword already consumed
    parser_consume(parser, TOKEN_FUNC, "Expected 'func' after 'async'");
    
    // Function name
    Token name = consume_decl_name_token(parser, "Expected function name");
    
    // Create async function node
    ASTNode* func = ast_create_async_function(name.text, true);
    parser->last_func_decl_async = true;
    func->data.async_func_decl.doc_comment = parser_take_pending_doc_comment(parser);
    
    // Generic parameters (if any)
    if (parser_check(parser, TOKEN_LESS)) {
        // Reuse existing generic parsing logic
        // func->data.async_func_decl.generic_params = parse_generic_params(parser);
    }
    
    // Function parameters
    parser_consume(parser, TOKEN_LPAREN, "Expected '(' after function name");
    
    // Parse parameters (similar to regular function)
    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        ParamMode mode = PARAM_MODE_DEFAULT;
        if (parser_match(parser, TOKEN_OWN))
            mode = PARAM_MODE_OWN;
        else if (parser_match(parser, TOKEN_REF))
            mode = PARAM_MODE_REF;
        else if (parser_match(parser, TOKEN_AMP)) {
            /* '&' / '&self' -- immutable borrow receiver. Caller-visible
             * mutation is spelled 'inout', never '&mut': the mutation is
             * value-result (copy-in/copy-out), not a live Rust-style borrow. */
            mode = PARAM_MODE_REF;
            if (parser_check(parser, TOKEN_IDENTIFIER)
                && parser->current_token.text != NULL
                && strcmp(parser->current_token.text, "mut") == 0) {
                parser_error(parser,
                    "'&mut' is not a binding mode in this language.\n"
                    "Reason: the mutation is value-result (copy-in/copy-out), "
                    "not a live borrow, so the '&mut' sigil is misleading.\n"
                    "Fix: use 'inout' (for example 'inout self' or 'inout xs').");
                parser_advance(parser); /* consume 'mut' to resync at name */
            }
        }
        else if (parser_check(parser, TOKEN_IDENTIFIER)
                 && parser->current_token.text != NULL
                 && strcmp(parser->current_token.text, "inout") == 0) {
            /* 'inout' -- the sole spelling for a value-result binding
             * (copy-in/copy-out), for parameters and the mutable receiver. */
            mode = PARAM_MODE_MUT_REF;
            parser_advance(parser);
        }

        Token param_name = consume_binding_name_token(parser, "Expected parameter name");

        FuncParam* param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup(param_name.text);
        param->mode = mode;

        // self parameter: no type annotation needed
        if (strcmp(param_name.text, "self") == 0
            && !parser_check(parser, TOKEN_COLON)) {
            param->type = NULL;
        } else {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after parameter name");
            ASTNode* param_type = parse_type(parser);
            param->type = param_type;
        }
        if (parser_match(parser, TOKEN_ASSIGN)) {
            parser_error(parser,
                "Default value arguments are reserved but not implemented.\n"
                "Reason: value defaults need call ABI, overload/dispatch, and named-argument interaction policy.\n"
                "Fix: use an overload or wrapper function.");
            ast_destroy(parser_parse_expression(parser));
        }
        
        parser_append_async_param(parser, func, param);
        
        if (!parser_match(parser, TOKEN_COMMA)) break;
    }
    
    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after parameters");
    
    // Return type
    if (parser_match(parser, TOKEN_ARROW)) {
        func->data.async_func_decl.return_type = parse_type(parser);
    }

    while (!parser_is_at_end(parser)) {
        if (func->data.async_func_decl.where_clause == NULL
            && parser_check(parser, TOKEN_WHERE)) {
            func->data.async_func_decl.where_clause = parse_where_clause(parser);
            continue;
        }
        if (!func->data.async_func_decl.has_effects_clause
            && parser_check(parser, TOKEN_WITH)) {
            parse_optional_effect_clause(parser,
                &func->data.async_func_decl.has_effects_clause,
                &func->data.async_func_decl.declared_effects);
            continue;
        }
        break;
    }
    
    // Function body
    parser->in_async_context = true;
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' before function body");
    func->data.async_func_decl.body = parser_parse_block(parser);
    parser->in_async_context = false;
    
    return func;
}

// Parse async block statement
ASTNode* parser_parse_async_block(Parser* parser)
{
    ASTNode* block = ast_create_async_block();

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after 'async'");

    bool saved_async = parser->in_async_context;
    parser->in_async_context = true;
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt != NULL) {
            parser_append_async_node(parser, &block->data.async_block.statements,
                                     &block->data.async_block.statement_count,
                                     &block->data.async_block.statement_capacity,
                                     stmt,
                                     "Out of memory while parsing async block");
        }
        if (parser->has_error) {
            parser_synchronize(parser);
        }
    }
    parser->in_async_context = saved_async;

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after async block");
    return block;
}

// Parse await expression
//
// Async-context enforcement is the semantic layer's responsibility
// (type_check rejects await when ctx->in_async_func is false, with its own
// regression case), so the parser does not duplicate that gate here. Keeping
// it out of the parser lets entry points and inline async blocks parse, then
// be validated where async provenance is actually modeled.
ASTNode* parser_parse_await_expression(Parser* parser)
{
    // 'await' keyword already consumed
    ASTNode* expression = parser_parse_expression(parser);

    return ast_create_await_expression(expression);
}

// Parse channel expression
ASTNode* parser_parse_channel_expression(Parser* parser)
{
    // Handle channel operations: <- channel or channel <- value
    
    if (parser_match(parser, TOKEN_CHANNEL_OP)) {
        Token op = parser->previous_token;
        // Receive: <-channel
        ASTNode* channel = parser_parse_primary(parser);
        ASTNode *recv = ast_create_channel_recv(channel);
        if (recv != NULL) {
            recv->line = op.line;
            recv->column = op.column;
        }
        return recv;
    }
    
    // Otherwise, parse as normal expression and check for send
    ASTNode* expr = parser_parse_primary(parser);
    
    if (parser_match(parser, TOKEN_CHANNEL_OP)) {
        Token op = parser->previous_token;
        // Send: channel <- value
        ASTNode* value = parser_parse_expression(parser);
        ASTNode *send = ast_create_channel_send(expr, value);
        if (send != NULL) {
            send->line = op.line;
            send->column = op.column;
        }
        return send;
    }
    
    return expr;
}

// Parse spawn expression
ASTNode* parser_parse_spawn_expression(Parser* parser)
{
    // 'spawn' keyword already consumed
    Token spawn_token = parser->previous_token;

    // Check for 'spawn blocking <expr>' — offload to blocking thread pool
    bool is_blocking = false;
    if (parser_check(parser, TOKEN_IDENTIFIER)
        && parser->current_token.text != NULL
        && strcmp(parser->current_token.text, "blocking") == 0) {
        parser_advance(parser);
        is_blocking = true;
    }

    // Can be either:
    // 1. spawn func_call()
    // 2. spawn async func() { ... }
    // 3. spawn blocking func_call()

    ASTNode *result;
    if (!is_blocking && parser_match(parser, TOKEN_ASYNC)) {
        parser_match(parser, TOKEN_FUNC);  // Optional 'func' keyword

        // Anonymous async function
        parser_consume(parser, TOKEN_LPAREN, "Expected '(' for spawn function");
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' for spawn function");

        parser_consume(parser, TOKEN_LBRACE, "Expected '{' for spawn body");
        ASTNode* body = parser_parse_block(parser);

        // Create anonymous function
        ASTNode* anon_func = ast_create_async_function("__anon", true);
        anon_func->data.async_func_decl.body = body;

        result = ast_create_spawn_expression(anon_func);
    } else {
        // Regular function call
        ASTNode* func_call = parser_parse_expression(parser);
        result = ast_create_spawn_expression(func_call);
    }

    if (result != NULL) {
        result->data.spawn_expr.is_blocking = is_blocking;
        result->line = spawn_token.line;
        result->column = spawn_token.column;
    }
    return result;
}

// Parse select statement
ASTNode* parser_parse_select_statement(Parser* parser)
{
    // 'select' keyword already consumed
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after 'select'");
    
    ASTNode* select_stmt = ast_create_select_statement();
    parser->in_select_statement = true;
    
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_CASE)) {
            // Parse select case
            ASTNode* case_node = NULL;
            
            if (parser_check(parser, TOKEN_CHANNEL_OP)) {
                // Receive case: case <-channel:
                parser_advance(parser);
                ASTNode* channel = parser_parse_expression(parser);
                case_node = ast_create_channel_recv(channel);
            } else {
                // Receive with assignment: case val = <-channel:
                Token var_name = consume_binding_name_token(parser, "Expected variable name");

                if (parser_match(parser, TOKEN_ASSIGN)) {
                    parser_consume(parser, TOKEN_CHANNEL_OP, "Expected '<-' in select case");
                    ASTNode* channel = parser_parse_expression(parser);
                    ASTNode* recv = ast_create_channel_recv(channel);

                    // Wrap in a let-like assignment: let var_name = <-channel
                    ASTNode* var_id = ast_create_identifier(var_name.text);
                    ASTNode* assign = calloc(1, sizeof(ASTNode));
                    assign->type = AST_ASSIGNMENT;
                    assign->data.assignment.target = var_id;
                    assign->data.assignment.value = recv;
                    case_node = assign;
                } else {
                    parser_error(parser, "Invalid select case");
                }
            }

            parser_consume(parser, TOKEN_COLON, "Expected ':' after select case");

            // Parse case body and attach to the case node
            ASTNode* body = parser_parse_statement(parser);
            if (parser->has_error) {
                parser_synchronize(parser);
            }

            // Wrap case_node + body in a block so both are preserved
            if (body != NULL && case_node != NULL) {
                ASTNode* block = ast_create_block();
                ast_add_statement(block, case_node);
                ast_add_statement(block, body);
                case_node = block;
            }
            
            // Add case to select statement
            parser_append_async_node(parser, &select_stmt->data.select_stmt.cases,
                                     &select_stmt->data.select_stmt.case_count,
                                     &select_stmt->data.select_stmt.case_capacity,
                                     case_node,
                                     "Out of memory while parsing select cases");
            
        } else if (parser_match(parser, TOKEN_DEFAULT)) {
            // Default case
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'default'");
            select_stmt->data.select_stmt.default_case = parser_parse_statement(parser);
            if (parser->has_error) {
                parser_synchronize(parser);
            }
        } else {
            parser_error(parser, "Expected 'case' or 'default' in select statement");
            parser_advance(parser);
        }
    }
    
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after select body");
    parser->in_select_statement = false;
    
    return select_stmt;
}
