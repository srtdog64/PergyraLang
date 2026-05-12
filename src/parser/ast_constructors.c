/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST node constructors
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>
static char *
ast_strdup_range(const char *src, size_t len)
{
    char *out = malloc(len + 1);
    if (out == NULL)
        return pergyra_strdup("");

    if (len > 0 && src != NULL) {
        memcpy(out, src, len);
    }
    out[len] = '\0';
    return out;
}

static char *
ast_unescape_string_literal(const char *value)
{
    size_t len;
    char *out;
    size_t i;
    size_t j = 0;

    if (value == NULL)
        return pergyra_strdup("");

    len = strlen(value);
    out = (char *)malloc(len + 1);
    if (out == NULL)
        return pergyra_strdup("");

    for (i = 0; i < len; i++) {
        if (value[i] == '\\' && i + 1 < len) {
            i++;
            switch (value[i]) {
            case 'n': out[j++] = '\n'; break;
            case 'r': out[j++] = '\r'; break;
            case 't': out[j++] = '\t'; break;
            case '\\': out[j++] = '\\'; break;
            case '"': out[j++] = '"'; break;
            case '0': out[j++] = '\0'; break;
            default:
                out[j++] = value[i];
                break;
            }
        } else {
            out[j++] = value[i];
        }
    }
    out[j] = '\0';
    return out;
}

ASTNode* ast_create_node(ASTNodeType type) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    if (!node) return NULL;
    node->type = type;
    node->access = ACCESS_PUBLIC;
    node->has_explicit_access = false;
    node->is_exported = false;
    node->has_explicit_export = false;
    node->is_async_decl = false;
    return node;
}


ASTNode* ast_create_program(void) {
    ASTNode* node = ast_create_node(AST_PROGRAM);
    node->data.program.statements = NULL;
    node->data.program.count = 0;
    return node;
}

// Function declaration
ASTNode* ast_create_function(const char* name) {
    ASTNode* node = ast_create_node(AST_FUNC_DECL);
    node->data.func_decl.name = pergyra_strdup(name);
    node->data.func_decl.params = NULL;
    node->data.func_decl.param_count = 0;
    node->data.func_decl.param_capacity = 0;
    node->data.func_decl.return_type = NULL;
    node->data.func_decl.body = NULL;
    node->data.func_decl.generic_params = NULL;
    node->data.func_decl.where_clause = NULL;
    node->data.func_decl.has_effects_clause = false;
    node->data.func_decl.declared_effects = 0;
    node->data.func_decl.access = ACCESS_PUBLIC;
    node->data.func_decl.has_explicit_access = false;
    node->data.func_decl.is_action = false;
    node->data.func_decl.required_abilities = NULL;
    node->data.func_decl.required_ability_count = 0;
    node->data.func_decl.required_ability_capacity = 0;
    node->data.func_decl.within_zone = NULL;
    node->data.func_decl.causes_effect = NULL;
    node->data.func_decl.authorized_by = NULL;
    node->data.func_decl.authorized_by_count = 0;
    node->data.func_decl.authorized_by_capacity = 0;
    return node;
}

// Class declaration
ASTNode* ast_create_class(const char* name) {
    ASTNode* node = ast_create_node(AST_CLASS_DECL);
    node->data.class_decl.name = pergyra_strdup(name);
    node->data.class_decl.fields = NULL;
    node->data.class_decl.field_count = 0;
    node->data.class_decl.field_capacity = 0;
    node->data.class_decl.methods = NULL;
    node->data.class_decl.method_count = 0;
    node->data.class_decl.method_capacity = 0;
    node->data.class_decl.generic_params = NULL;
    node->data.class_decl.where_clause = NULL;
    node->data.class_decl.is_struct = false;
    node->data.class_decl.nominal_kind = NOMINAL_DECL_CLASS;
    return node;
}

ASTNode* ast_create_subject(const char* name) {
    ASTNode* node = ast_create_class(name);
    if (node) {
        node->data.class_decl.nominal_kind = NOMINAL_DECL_SUBJECT;
    }
    return node;
}

ASTNode* ast_create_vessel(const char* name) {
    ASTNode* node = ast_create_struct(name);
    if (node) {
        node->data.class_decl.nominal_kind = NOMINAL_DECL_VESSEL;
    }
    return node;
}

// Struct declaration
ASTNode* ast_create_struct(const char* name) {
    ASTNode* node = ast_create_class(name);
    if (node) {
        node->data.class_decl.is_struct = true;
        node->data.class_decl.nominal_kind = NOMINAL_DECL_STRUCT;
    }
    return node;
}

ASTNode* ast_create_object(const char* name) {
    ASTNode* node = ast_create_struct(name);
    if (node) {
        /* object is a local/internal projection contract, not a struct alias */
        node->data.class_decl.nominal_kind = NOMINAL_DECL_OBJECT;
    }
    return node;
}

ASTNode* ast_create_tobject(const char* name) {
    ASTNode* node = ast_create_struct(name);
    if (node) {
        /* tobject is a boundary transfer contract, not a struct alias */
        node->data.class_decl.nominal_kind = NOMINAL_DECL_TOBJECT;
    }
    return node;
}

ASTNode* ast_create_extern_block(const char* abi) {
    ASTNode* node = ast_create_node(AST_EXTERN_BLOCK);
    if (!node) return NULL;
    node->data.extern_block.abi = pergyra_strdup(abi);
    node->data.extern_block.declarations = NULL;
    node->data.extern_block.count = 0;
    return node;
}

// Let declaration
ASTNode* ast_create_let_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_LET_DECL);
    node->data.let_decl.name = pergyra_strdup(name);
    node->data.let_decl.type = NULL;
    node->data.let_decl.initializer = NULL;
    node->data.let_decl.is_mutable = false;
    node->data.let_decl.is_alias = false;
    return node;
}

ASTNode* ast_create_type_alias(const char* name, ASTNode* target_type) {
    ASTNode* node = ast_create_node(AST_TYPE_ALIAS);
    if (!node) return NULL;
    node->data.type_alias.name = pergyra_strdup(name);
    node->data.type_alias.target_type = target_type;
    return node;
}

// With statement
ASTNode* ast_create_with_statement(void) {
    ASTNode* node = ast_create_node(AST_WITH_STMT);
    node->data.with_stmt.slot_type = NULL;
    node->data.with_stmt.alias = NULL;
    node->data.with_stmt.body = NULL;
    node->data.with_stmt.is_secure = false;
    node->data.with_stmt.security_level = NULL;
    return node;
}

ASTNode* ast_create_parallel_block(void) {
    ASTNode* node = ast_create_node(AST_PARALLEL_BLOCK);
    node->data.parallel.tasks = NULL;
    node->data.parallel.task_count = 0;
    return node;
}

ASTNode* ast_create_block(void) {
    ASTNode* node = ast_create_node(AST_BLOCK);
    node->data.block.statements = NULL;
    node->data.block.count = 0;
    node->data.block.is_pin_block = false;
    node->data.block.pin_view_is_write = false;
    node->data.block.pin_source_name = NULL;
    node->data.block.pin_view_name = NULL;
    return node;
}

ASTNode* ast_create_for_loop(void) {
    ASTNode* node = ast_create_node(AST_FOR_LOOP);
    node->data.for_loop.label = NULL;
    node->data.for_loop.variable = NULL;
    node->data.for_loop.range_start = NULL;
    node->data.for_loop.range_end = NULL;
    node->data.for_loop.iterable = NULL;
    node->data.for_loop.body = NULL;
    return node;
}

ASTNode* ast_create_while_loop(void) {
    ASTNode* node = ast_create_node(AST_WHILE_LOOP);
    node->data.while_loop.label = NULL;
    node->data.while_loop.condition = NULL;
    node->data.while_loop.body = NULL;
    return node;
}

// Match statement
ASTNode* ast_create_match_statement(void) {
    ASTNode* node = ast_create_node(AST_MATCH_STMT);
    node->data.match_stmt.subject = NULL;
    node->data.match_stmt.cases = NULL;
    node->data.match_stmt.case_count = 0;
    node->data.match_stmt.case_capacity = 0;
    node->data.match_stmt.default_body = NULL;
    return node;
}

// match case
ASTNode* ast_create_match_case(void) {
    ASTNode* node = ast_create_node(AST_MATCH_CASE);
    node->data.match_case.pattern = NULL;
    node->data.match_case.patterns = NULL;
    node->data.match_case.pattern_count = 0;
    node->data.match_case.pattern_capacity = 0;
    node->data.match_case.guard = NULL;
    node->data.match_case.body = NULL;
    return node;
}

ASTNode* ast_create_import_declaration(const char* path) {
    ASTNode* node = ast_create_node(AST_IMPORT_DECL);
    node->data.import_decl.path = pergyra_strdup(path);
    return node;
}

ASTNode* ast_create_namespace_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_NAMESPACE_DECL);
    if (!node) return NULL;
    node->data.namespace_decl.name = pergyra_strdup(name);
    node->data.namespace_decl.statements = NULL;
    node->data.namespace_decl.count = 0;
    return node;
}

ASTNode* ast_create_unsafe_block(ASTNode* body) {
    ASTNode* node = ast_create_node(AST_UNSAFE_BLOCK);
    node->data.unsafe_block.body = body;
    return node;
}

ASTNode* ast_create_defer_statement(ASTNode* body) {
    ASTNode* node = ast_create_node(AST_DEFER_STMT);
    node->data.defer_stmt.body = body;
    return node;
}

ASTNode* ast_create_bind_statement(const char* party_var, const char* slot_name, const char* role_name) {
    ASTNode* node = ast_create_node(AST_BIND_STMT);
    node->data.bind_stmt.party_var = pergyra_strdup(party_var);
    node->data.bind_stmt.slot_name = pergyra_strdup(slot_name);
    node->data.bind_stmt.role_name = pergyra_strdup(role_name);
    return node;
}

// If statement
ASTNode* ast_create_if_statement(void) {
    ASTNode* node = ast_create_node(AST_IF_STMT);
    node->data.if_stmt.condition = NULL;
    node->data.if_stmt.then_branch = NULL;
    node->data.if_stmt.else_branch = NULL;
    return node;
}

// Return statement
ASTNode* ast_create_return_statement(void) {
    ASTNode* node = ast_create_node(AST_RETURN);
    node->data.return_stmt.value = NULL;
    return node;
}

// Expression constructors
// Binary operation
ASTNode* ast_create_binary(ASTNode* left, Token op, ASTNode* right) {
    ASTNode* node = ast_create_node(AST_BINARY);
    node->data.binary.left = left;
    node->data.binary.op = op;
    node->data.binary.right = right;
    return node;
}

// Unary operation
ASTNode* ast_create_unary(Token op, ASTNode* operand) {
    ASTNode* node = ast_create_node(AST_UNARY);
    node->data.unary.op = op;
    node->data.unary.operand = operand;
    return node;
}

// Function call
ASTNode* ast_create_call(ASTNode* callee) {
    ASTNode* node = ast_create_node(AST_CALL);
    node->data.call.callee = callee;
    node->data.call.arguments = NULL;
    node->data.call.arg_names = NULL;
    node->data.call.arg_count = 0;
    node->data.call.generic_args = NULL;
    return node;
}

// Member access
ASTNode* ast_create_member_access(ASTNode* object, const char* member) {
    ASTNode* node = ast_create_node(AST_MEMBER_ACCESS);
    node->data.member.object = object;
    node->data.member.name = pergyra_strdup(member);
    return node;
}

// Array access
ASTNode* ast_create_array_access(ASTNode* array, ASTNode* index) {
    ASTNode* node = ast_create_node(AST_ARRAY_ACCESS);
    node->data.array_access.array = array;
    node->data.array_access.index = index;
    return node;
}

// Assignment
ASTNode* ast_create_assignment(ASTNode* target, ASTNode* value) {
    ASTNode* node = ast_create_node(AST_ASSIGNMENT);
    node->data.assignment.target = target;
    node->data.assignment.value = value;
    return node;
}

// Literal constructors
// Number literal
ASTNode* ast_create_number(const char* value) {
    ASTNode* node = ast_create_node(AST_NUMBER);
    node->data.number.is_long = false;
    if (value != NULL) {
        size_t len = strlen(value);
        if (len > 0 && value[len - 1] == 'L') {
            node->data.number.is_long = true;
            /* strtod stops at 'L' automatically, so no need to strip. */
        }
    }
    node->data.number.value = strtod(value, NULL);
    return node;
}

// String literal
ASTNode* ast_create_string(const char* value) {
    ASTNode* node = ast_create_node(AST_STRING);
    // Strip quote delimiters before unescaping.
    size_t len = strlen(value);
    const char *inner = value;
    size_t inner_len = len;
    bool is_multiline = len >= 6 &&
        strncmp(value, "\"\"\"", 3) == 0 &&
        strncmp(value + len - 3, "\"\"\"", 3) == 0;

    if (is_multiline) {
        inner = value + 3;
        inner_len = len - 6;
    } else if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
        inner = value + 1;
        inner_len = len - 2;
    }

    if (is_multiline) {
        node->data.string.value = ast_strdup_range(inner, inner_len);
        if (node->data.string.value == NULL) {
            node->data.string.value = pergyra_strdup("");
        }
    } else if (inner_len > 0) {
        char *raw = pergyra_strndup(inner, inner_len);
        node->data.string.value = ast_unescape_string_literal(raw);
        free(raw);
    } else {
        node->data.string.value = ast_unescape_string_literal("");
    }
    return node;
}

ASTNode* ast_create_boolean(bool value) {
    ASTNode* node = ast_create_node(AST_BOOLEAN);
    node->data.boolean.value = value;
    return node;
}

// Identifier
ASTNode* ast_create_identifier(const char* name) {
    ASTNode* node = ast_create_node(AST_IDENTIFIER);
    node->data.identifier.name = pergyra_strdup(name);
    return node;
}

// Type reference
ASTNode* ast_create_type(const char* name) {
    ASTNode* node = ast_create_node(AST_TYPE);
    node->data.type.name = pergyra_strdup(name);
    node->data.type.generic_args = NULL;
    node->data.type.tuple_elements = NULL;
    node->data.type.tuple_element_count = 0;
    return node;
}
