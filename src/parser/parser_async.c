/*
 * Copyright (c) 2025 Pergyra Language Project
 * Async parsing support for Pergyra parser
 * BSD Style + C# naming conventions
 */

#include "parser_internal.h"

// Parse async function declaration
ASTNode* parser_parse_async_function(Parser* parser)
{
    // 'async' keyword already consumed
    parser_consume(parser, TOKEN_FUNC, "Expected 'func' after 'async'");
    
    // Function name
    Token name = consume_name_token(parser, "Expected function name");
    
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

        Token param_name = consume_name_token(parser, "Expected parameter name");

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
        
        func->data.async_func_decl.param_count++;
        func->data.async_func_decl.params = realloc(
            func->data.async_func_decl.params,
            func->data.async_func_decl.param_count * sizeof(FuncParam*)
        );
        func->data.async_func_decl.params[func->data.async_func_decl.param_count - 1] = param;
        
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
            block->data.async_block.statement_count++;
            block->data.async_block.statements = realloc(
                block->data.async_block.statements,
                block->data.async_block.statement_count * sizeof(ASTNode*)
            );
            block->data.async_block.statements[
                block->data.async_block.statement_count - 1] = stmt;
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
ASTNode* parser_parse_await_expression(Parser* parser)
{
    if (!parser->in_async_context) {
        parser_error(parser, "'await' can only be used in async context");
    }
    
    // 'await' keyword already consumed
    ASTNode* expression = parser_parse_expression(parser);
    
    return ast_create_await_expression(expression);
}

// Parse channel expression
ASTNode* parser_parse_channel_expression(Parser* parser)
{
    // Handle channel operations: <- channel or channel <- value
    
    if (parser_match(parser, TOKEN_CHANNEL_OP)) {
        // Receive: <-channel
        ASTNode* channel = parser_parse_primary(parser);
        return ast_create_channel_recv(channel);
    }
    
    // Otherwise, parse as normal expression and check for send
    ASTNode* expr = parser_parse_primary(parser);
    
    if (parser_match(parser, TOKEN_CHANNEL_OP)) {
        // Send: channel <- value
        ASTNode* value = parser_parse_expression(parser);
        return ast_create_channel_send(expr, value);
    }
    
    return expr;
}

// Parse spawn expression
ASTNode* parser_parse_spawn_expression(Parser* parser)
{
    // 'spawn' keyword already consumed

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

    if (result != NULL)
        result->data.spawn_expr.is_blocking = is_blocking;
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
                Token var_name = consume_name_token(parser, "Expected variable name");

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
            select_stmt->data.select_stmt.case_count++;
            select_stmt->data.select_stmt.cases = realloc(
                select_stmt->data.select_stmt.cases,
                select_stmt->data.select_stmt.case_count * sizeof(ASTNode*)
            );
            select_stmt->data.select_stmt.cases[select_stmt->data.select_stmt.case_count - 1] = case_node;
            
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

