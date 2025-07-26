/*
 * Copyright (c) 2025 Pergyra Language Project
 * Async parsing support for Pergyra parser
 * BSD Style + C# naming conventions
 */

#include "parser.h"
#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declarations
static ASTNode* parse_type(Parser* parser);
static ASTNode* parse_function_body(Parser* parser);

// Parse async function declaration
ASTNode* parser_parse_async_function(Parser* parser)
{
    // 'async' keyword already consumed
    parser_consume(parser, TOKEN_FUNC, "Expected 'func' after 'async'");
    
    // Function name
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected function name");
    
    // Create async function node
    ASTNode* func = ast_create_async_function(name.text, true);
    
    // Generic parameters (if any)
    if (parser_check(parser, TOKEN_LESS)) {
        // Reuse existing generic parsing logic
        // func->data.async_func_decl.generic_params = parse_generic_params(parser);
    }
    
    // Function parameters
    parser_consume(parser, TOKEN_LPAREN, "Expected '(' after function name");
    
    // Parse parameters (similar to regular function)
    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        Token param_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected parameter name");
        parser_consume(parser, TOKEN_COLON, "Expected ':' after parameter name");
        ASTNode* param_type = parse_type(parser);
        
        // Add parameter to function
        FuncParam* param = calloc(1, sizeof(FuncParam));
        param->name = strdup(param_name.text);
        param->type = param_type;
        
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
    
    // Function body
    parser->in_async_context = true;
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' before function body");
    func->data.async_func_decl.body = parser_parse_block(parser);
    parser->in_async_context = false;
    
    return func;
}

// Parse actor declaration
ASTNode* parser_parse_actor_declaration(Parser* parser)
{
    // 'actor' keyword already consumed
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected actor name");
    
    ASTNode* actor = ast_create_actor(name.text);
    
    // Generic parameters (if any)
    if (parser_check(parser, TOKEN_LESS)) {
        // actor->data.actor_decl.generic_params = parse_generic_params(parser);
    }
    
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after actor name");
    
    // Parse actor body (fields and methods)
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        AccessModifier access = ACCESS_PUBLIC;
        
        // Access modifiers
        if (parser_match(parser, TOKEN_PUBLIC)) {
            access = ACCESS_PUBLIC;
        } else if (parser_match(parser, TOKEN_PRIVATE)) {
            access = ACCESS_PRIVATE;
        }
        
        // Fields or methods
        if (parser_check(parser, TOKEN_LET)) {
            parser_advance(parser);
            
            // Field
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected field name");
            parser_consume(parser, TOKEN_COLON, "Expected ':' after field name");
            ASTNode* field_type = parse_type(parser);
            
            ClassField* field = calloc(1, sizeof(ClassField));
            field->name = strdup(field_name.text);
            field->type = field_type;
            field->access = access;
            field->is_mutable = true;  // Actor fields are mutable by default
            
            // Add field
            actor->data.actor_decl.field_count++;
            actor->data.actor_decl.fields = realloc(
                actor->data.actor_decl.fields,
                actor->data.actor_decl.field_count * sizeof(ClassField*)
            );
            actor->data.actor_decl.fields[actor->data.actor_decl.field_count - 1] = field;
            
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after field declaration");
            
        } else if (parser_match(parser, TOKEN_FUNC)) {
            // Method (implicitly async in actors)
            ASTNode* method = parser_parse_async_function(parser);
            
            // Add method
            actor->data.actor_decl.method_count++;
            actor->data.actor_decl.methods = realloc(
                actor->data.actor_decl.methods,
                actor->data.actor_decl.method_count * sizeof(ASTNode*)
            );
            actor->data.actor_decl.methods[actor->data.actor_decl.method_count - 1] = method;
        }
    }
    
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after actor body");
    
    return actor;
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
    
    // Can be either:
    // 1. spawn func_call()
    // 2. spawn async func() { ... }
    
    if (parser_match(parser, TOKEN_ASYNC)) {
        parser_match(parser, TOKEN_FUNC);  // Optional 'func' keyword
        
        // Anonymous async function
        parser_consume(parser, TOKEN_LPAREN, "Expected '(' for spawn function");
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' for spawn function");
        
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' for spawn body");
        ASTNode* body = parser_parse_block(parser);
        
        // Create anonymous function
        ASTNode* anon_func = ast_create_async_function("__anon", true);
        anon_func->data.async_func_decl.body = body;
        
        return ast_create_spawn_expression(anon_func);
    } else {
        // Regular function call
        ASTNode* func_call = parser_parse_expression(parser);
        return ast_create_spawn_expression(func_call);
    }
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
                // Receive case: case value = <-channel:
                parser_advance(parser);
                ASTNode* channel = parser_parse_expression(parser);
                
                // TODO: Create proper case node with pattern
                case_node = ast_create_channel_recv(channel);
            } else {
                // Send case or other
                Token var_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected variable name");
                
                if (parser_match(parser, TOKEN_ASSIGN)) {
                    parser_consume(parser, TOKEN_CHANNEL_OP, "Expected '<-' in select case");
                    ASTNode* channel = parser_parse_expression(parser);
                    
                    // TODO: Create receive case with assignment
                    case_node = ast_create_channel_recv(channel);
                } else {
                    parser_error(parser, "Invalid select case");
                }
            }
            
            parser_consume(parser, TOKEN_COLON, "Expected ':' after select case");
            
            // Parse case body
            ASTNode* body = parser_parse_statement(parser);
            
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
        } else {
            parser_error(parser, "Expected 'case' or 'default' in select statement");
            parser_advance(parser);
        }
    }
    
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after select body");
    parser->in_select_statement = false;
    
    return select_stmt;
}

// Helper function to parse type (stub for now)
static ASTNode* parse_type(Parser* parser)
{
    // This should be implemented in the main parser
    // For now, just parse identifier
    Token type_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected type name");
    
    ASTNode* type = ast_create_type(type_name.text);
    
    // Handle generic types like Channel<T>
    if (parser_match(parser, TOKEN_LESS)) {
        // Parse generic arguments
        ASTNode* element_type = parse_type(parser);
        parser_consume(parser, TOKEN_GREATER, "Expected '>' after generic type");
        
        if (strcmp(type_name.text, "Channel") == 0) {
            return ast_create_channel_type(element_type);
        } else if (strcmp(type_name.text, "Future") == 0) {
            return ast_create_future_type(element_type);
        }
        
        // Regular generic type
        type->data.type.generic_args = calloc(1, sizeof(GenericParams));
        type->data.type.generic_args->count = 1;
        // TODO: Properly handle generic arguments
    }
    
    return type;
}
