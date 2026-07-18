#include "mir_json_expression_graph.h"

#include "mir.h"
#include "mir_json_dump_internal.h"

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"

typedef struct {
    const char *kind;
    char *text;
    const char *call_target_kind;
    const char *call_target_name;
    int left;
    int right;
} MIRJsonExpressionGraphNode;

typedef struct {
    MIRJsonExpressionGraphNode *nodes;
    size_t count;
    size_t capacity;
} MIRJsonExpressionGraph;

static void
mir_json_expression_graph_dispose(MIRJsonExpressionGraph *graph)
{
    if (graph == NULL)
        return;
    for (size_t i = 0; i < graph->count; i++)
        free(graph->nodes[i].text);
    free(graph->nodes);
    memset(graph, 0, sizeof(*graph));
}

static int
mir_json_expression_graph_append(MIRJsonExpressionGraph *graph,
                                 const char *kind,
                                 char *text,
                                 int left,
                                 int right,
                                 const char *target_kind,
                                 const char *target_name)
{
    MIRJsonExpressionGraphNode *grown;
    size_t capacity;

    if (graph == NULL || kind == NULL || text == NULL)
        return -1;
    if (graph->count > INT_MAX)
        return -1;
    if (graph->count == graph->capacity) {
        capacity = graph->capacity == 0 ? 8 : graph->capacity * 2;
        if (capacity < graph->capacity
            || capacity > SIZE_MAX / sizeof(*graph->nodes)) {
            return -1;
        }
        grown = realloc(graph->nodes, capacity * sizeof(*graph->nodes));
        if (grown == NULL)
            return -1;
        graph->nodes = grown;
        graph->capacity = capacity;
    }
    graph->nodes[graph->count] = (MIRJsonExpressionGraphNode){
        kind,
        text,
        target_kind != NULL ? target_kind : "none",
        target_name != NULL ? target_name : "",
        left,
        right
    };
    return (int)graph->count++;
}

static char *
mir_json_expression_graph_copy_text(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL)
        return NULL;
    length = strlen(text);
    copy = malloc(length + 1);
    if (copy != NULL)
        memcpy(copy, text, length + 1);
    return copy;
}

static char *
mir_json_expression_graph_capture_text(ASTNode *expr)
{
    char *text = ast_capture_inline(expr);
    size_t length;

    if (text == NULL || expr == NULL
        || (expr->type != AST_BINARY && expr->type != AST_UNARY)) {
        return text;
    }
    length = strlen(text);
    if (length >= 2 && text[0] == '(' && text[length - 1] == ')') {
        memmove(text, text + 1, length - 2);
        text[length - 2] = '\0';
    }
    return text;
}

static const char *
mir_json_binary_graph_kind(PgyTokenType type)
{
    switch (type) {
    case TOKEN_OR: return "logical_or";
    case TOKEN_AND: return "logical_and";
    case TOKEN_EQUAL: return "equality";
    case TOKEN_NOT_EQUAL: return "inequality";
    case TOKEN_LESS: return "less";
    case TOKEN_LESS_EQUAL: return "less_equal";
    case TOKEN_GREATER: return "greater";
    case TOKEN_GREATER_EQUAL: return "greater_equal";
    case TOKEN_PLUS: return "add";
    case TOKEN_MINUS: return "subtract";
    case TOKEN_STAR: return "multiply";
    case TOKEN_SLASH: return "divide";
    case TOKEN_PERCENT: return "modulo";
    default: return NULL;
    }
}

static const char *
mir_json_unary_graph_kind(PgyTokenType type)
{
    if (type == TOKEN_NOT)
        return "logical_not";
    if (type == TOKEN_MINUS)
        return "negate";
    if (type == TOKEN_QUESTION)
        return "try";
    return NULL;
}

static int mir_json_expression_graph_build(MIRJsonExpressionGraph *graph,
                                           ASTNode *expr);

static char *
mir_json_expression_graph_generic_callee_text(const char *callee,
                                              const char *actual,
                                              bool continues)
{
    size_t callee_length;
    size_t actual_length;
    size_t stem_length;
    size_t separator_length = continues ? 2 : 1;
    size_t base_length;
    char *text;

    if (callee == NULL || actual == NULL)
        return NULL;
    callee_length = strlen(callee);
    actual_length = strlen(actual);
    if (continues && (callee_length == 0
        || callee[callee_length - 1] != '>')) {
        return NULL;
    }
    stem_length = continues ? callee_length - 1 : callee_length;
    if (stem_length > SIZE_MAX - separator_length - 2)
        return NULL;
    base_length = stem_length + separator_length + 2;
    if (actual_length > SIZE_MAX - base_length)
        return NULL;
    text = malloc(base_length + actual_length);
    if (text == NULL)
        return NULL;
    memcpy(text, callee, stem_length);
    memcpy(text + stem_length, continues ? ", " : "<", separator_length);
    memcpy(text + stem_length + separator_length, actual, actual_length);
    text[stem_length + separator_length + actual_length] = '>';
    text[stem_length + separator_length + actual_length + 1] = '\0';
    return text;
}

static int
mir_json_expression_graph_build_generic_callee(
    MIRJsonExpressionGraph *graph,
    ASTNode *expr,
    int callee_root)
{
    size_t generic_count = ast_call_generic_arg_count(expr);

    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *param = ast_call_generic_arg(expr, i);
        ASTNode *type_node;
        const char *type_name;
        char *actual_text;
        char *callee_text;
        int actual_root;
        int generic_root;

        if (param == NULL)
            return -1;
        type_node = ast_generic_param_constraint(param);
        if (type_node == NULL)
            type_node = ast_generic_param_default_type(param);
        type_name = ast_generic_param_name(param);
        actual_text = type_node != NULL
            ? ast_capture_inline(type_node)
            : mir_json_expression_graph_copy_text(type_name);
        actual_root = mir_json_expression_graph_append(
            graph, "generic_type_actual", actual_text,
            -1, -1, "none", "");
        if (actual_root < 0) {
            free(actual_text);
            return -1;
        }
        callee_text = mir_json_expression_graph_generic_callee_text(
            graph->nodes[callee_root].text, actual_text, i > 0);
        generic_root = mir_json_expression_graph_append(
            graph, "generic_callee", callee_text,
            callee_root, actual_root, "none", "");
        if (generic_root < 0) {
            free(callee_text);
            return -1;
        }
        callee_root = generic_root;
    }
    return callee_root;
}

static char *
mir_json_expression_graph_array_text(const char *prefix,
                                     const char *element)
{
    size_t prefix_length;
    size_t element_length;
    size_t separator_length;
    size_t base_length;
    char *text;

    if (prefix == NULL || element == NULL)
        return NULL;
    prefix_length = strlen(prefix);
    element_length = strlen(element);
    if (prefix_length < 2 || prefix[0] != '['
        || prefix[prefix_length - 1] != ']') {
        return NULL;
    }
    separator_length = prefix_length == 2 ? 0 : 2;
    if (prefix_length > SIZE_MAX - separator_length)
        return NULL;
    base_length = prefix_length + separator_length;
    if (base_length == SIZE_MAX)
        return NULL;
    if (element_length > SIZE_MAX - base_length - 1)
        return NULL;
    text = malloc(base_length + element_length + 1);
    if (text == NULL)
        return NULL;
    memcpy(text, prefix, prefix_length - 1);
    if (separator_length != 0)
        memcpy(text + prefix_length - 1, ", ", separator_length);
    memcpy(text + prefix_length - 1 + separator_length,
           element, element_length);
    text[prefix_length - 1 + separator_length + element_length] = ']';
    text[prefix_length + separator_length + element_length] = '\0';
    return text;
}

static int
mir_json_expression_graph_build_array(MIRJsonExpressionGraph *graph,
                                      ASTNode *expr)
{
    int literal_root;
    char *text = mir_json_expression_graph_copy_text("[]");

    literal_root = mir_json_expression_graph_append(
        graph, "array_literal", text, -1, -1, "none", "");
    if (literal_root < 0) {
        free(text);
        return -1;
    }
    for (size_t i = 0; i < ast_array_literal_count(expr); i++) {
        ASTNode *element = ast_array_literal_element(expr, i);
        int element_root = mir_json_expression_graph_build(graph, element);

        if (element_root < 0)
            return -1;
        text = mir_json_expression_graph_array_text(
            graph->nodes[literal_root].text,
            graph->nodes[element_root].text);
        literal_root = mir_json_expression_graph_append(
            graph, "array_element", text, literal_root, element_root,
            "none", "");
        if (literal_root < 0) {
            free(text);
            return -1;
        }
    }
    return literal_root;
}

static int
mir_json_expression_graph_build_call(MIRJsonExpressionGraph *graph,
                                     ASTNode *expr)
{
    ASTNode *callee = ast_call_callee(expr);
    ASTNode borrowed;
    ASTNode **arguments = NULL;
    size_t argument_count = 0;
    const char *target_kind = "none";
    const char *target_name = "";
    int call_root;
    int callee_root;
    char *text;

    if (callee == NULL)
        return -1;
    callee_root = mir_json_expression_graph_build(graph, callee);
    if (callee_root < 0)
        return -1;
    callee_root = mir_json_expression_graph_build_generic_callee(
        graph, expr, callee_root);
    if (callee_root < 0)
        return -1;
    if (callee->type == AST_IDENTIFIER) {
        target_kind = "direct";
        target_name = ast_identifier_name(callee);
    } else if (callee->type == AST_MEMBER_ACCESS) {
        target_kind = "member";
        target_name = ast_member_name(callee);
    }
    ast_init_call_borrowed_view(
        &borrowed, callee, NULL, 0, ast_call_generic_args(expr));
    text = ast_capture_inline(&borrowed);
    call_root = mir_json_expression_graph_append(
        graph, "call", text, callee_root, -1, target_kind, target_name);
    if (call_root < 0) {
        free(text);
        return -1;
    }

    arguments = ast_call_arguments(expr, &argument_count);
    for (size_t i = 0; i < argument_count; i++) {
        int argument_root = mir_json_expression_graph_build(graph, arguments[i]);
        if (argument_root < 0)
            return -1;
        ast_init_call_borrowed_view(
            &borrowed, callee, arguments, i + 1,
            ast_call_generic_args(expr));
        text = ast_capture_inline(&borrowed);
        call_root = mir_json_expression_graph_append(
            graph, "call_argument", text, call_root, argument_root,
            "none", "");
        if (call_root < 0) {
            free(text);
            return -1;
        }
    }
    return call_root;
}

static char *
mir_json_expression_graph_struct_literal_text(const char *name)
{
    size_t name_length;
    char *text;

    if (name == NULL)
        return NULL;
    name_length = strlen(name);
    if (name_length > SIZE_MAX - 5)
        return NULL;
    text = malloc(name_length + 5);
    if (text == NULL)
        return NULL;
    memcpy(text, name, name_length);
    memcpy(text + name_length, " { }", 5);
    return text;
}

static char *
mir_json_expression_graph_struct_binding_text(const char *name,
                                               const char *value)
{
    size_t name_length;
    size_t value_length;
    char *text;

    if (name == NULL || value == NULL)
        return NULL;
    name_length = strlen(name);
    value_length = strlen(value);
    if (name_length > SIZE_MAX - 3
        || value_length > SIZE_MAX - name_length - 3) {
        return NULL;
    }
    text = malloc(name_length + value_length + 3);
    if (text == NULL)
        return NULL;
    memcpy(text, name, name_length);
    memcpy(text + name_length, ": ", 2);
    memcpy(text + name_length + 2, value, value_length + 1);
    return text;
}

static char *
mir_json_expression_graph_struct_field_text(const char *prefix,
                                             const char *binding)
{
    size_t prefix_length;
    size_t binding_length;
    size_t separator_length;
    size_t stem_length;
    char *text;

    if (prefix == NULL || binding == NULL)
        return NULL;
    prefix_length = strlen(prefix);
    binding_length = strlen(binding);
    if (prefix_length < 4
        || prefix[prefix_length - 2] != ' '
        || prefix[prefix_length - 1] != '}') {
        return NULL;
    }
    stem_length = prefix_length - 2;
    separator_length = prefix[stem_length - 1] == '{' ? 1 : 2;
    if (stem_length > SIZE_MAX - separator_length - 3
        || binding_length > SIZE_MAX - stem_length - separator_length - 3) {
        return NULL;
    }
    text = malloc(stem_length + separator_length + binding_length + 3);
    if (text == NULL)
        return NULL;
    memcpy(text, prefix, stem_length);
    memcpy(text + stem_length,
           separator_length == 1 ? " " : ", ", separator_length);
    memcpy(text + stem_length + separator_length, binding, binding_length);
    memcpy(text + stem_length + separator_length + binding_length, " }", 3);
    return text;
}

static int
mir_json_expression_graph_build_struct(MIRJsonExpressionGraph *graph,
                                       ASTNode *expr)
{
    ASTNode *callee = ast_call_callee(expr);
    const char *name;
    int literal_root;
    int nominal_root;
    char *text;

    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return -1;
    name = ast_identifier_name(callee);
    nominal_root = mir_json_expression_graph_build(graph, callee);
    if (nominal_root < 0)
        return -1;
    text = mir_json_expression_graph_struct_literal_text(name);
    literal_root = mir_json_expression_graph_append(
        graph, "struct_literal", text, nominal_root, -1, "none", "");
    if (literal_root < 0) {
        free(text);
        return -1;
    }
    for (size_t i = 0; i < ast_call_arg_count(expr); i++) {
        const char *field_name = ast_call_argument_name(expr, i);
        ASTNode *field_value = ast_call_argument(expr, i);
        char *field_name_text;
        int field_name_root;
        int field_value_root;
        int binding_root;

        if (field_name == NULL || field_value == NULL)
            return -1;
        field_name_text = mir_json_expression_graph_copy_text(field_name);
        field_name_root = mir_json_expression_graph_append(
            graph, "struct_field_name", field_name_text,
            -1, -1, "none", "");
        if (field_name_root < 0) {
            free(field_name_text);
            return -1;
        }
        field_value_root = mir_json_expression_graph_build(graph, field_value);
        if (field_value_root < 0)
            return -1;
        text = mir_json_expression_graph_struct_binding_text(
            field_name, graph->nodes[field_value_root].text);
        binding_root = mir_json_expression_graph_append(
            graph, "struct_field_binding", text,
            field_name_root, field_value_root, "none", "");
        if (binding_root < 0) {
            free(text);
            return -1;
        }
        text = mir_json_expression_graph_struct_field_text(
            graph->nodes[literal_root].text,
            graph->nodes[binding_root].text);
        literal_root = mir_json_expression_graph_append(
            graph, "struct_field", text,
            literal_root, binding_root, "none", "");
        if (literal_root < 0) {
            free(text);
            return -1;
        }
    }
    return literal_root;
}

static int
mir_json_expression_graph_build(MIRJsonExpressionGraph *graph, ASTNode *expr)
{
    const char *kind = NULL;
    int left = -1;
    int right = -1;
    char *text;

    if (graph == NULL || expr == NULL)
        return -1;
    switch (expr->type) {
    case AST_IDENTIFIER:
        kind = "leaf";
        break;
    case AST_NUMBER:
        kind = ast_number_is_long(expr)
            ? "long_literal"
            : ast_number_is_float(expr)
                ? "float_literal"
                : "integer_literal";
        break;
    case AST_STRING:
        kind = "string_literal";
        break;
    case AST_BOOLEAN:
        kind = "bool_literal";
        break;
    case AST_BINARY: {
        Token token = ast_binary_operator(expr);
        kind = mir_json_binary_graph_kind(token.type);
        if (kind == NULL)
            return -1;
        left = mir_json_expression_graph_build(graph, ast_binary_left(expr));
        right = mir_json_expression_graph_build(graph, ast_binary_right(expr));
        if (left < 0 || right < 0)
            return -1;
        break;
    }
    case AST_UNARY: {
        Token token = ast_unary_operator(expr);
        kind = mir_json_unary_graph_kind(token.type);
        if (kind == NULL)
            return -1;
        left = mir_json_expression_graph_build(graph, ast_unary_operand(expr));
        if (left < 0)
            return -1;
        break;
    }
    case AST_CALL:
        if (ast_call_uses_braced_initializer_syntax(expr))
            return mir_json_expression_graph_build_struct(graph, expr);
        return mir_json_expression_graph_build_call(graph, expr);
    case AST_ARRAY_LITERAL:
        return mir_json_expression_graph_build_array(graph, expr);
    case AST_MEMBER_ACCESS:
    {
        char *member_text;

        left = mir_json_expression_graph_build(graph, ast_member_object(expr));
        member_text = mir_json_expression_graph_copy_text(
            ast_member_name(expr));
        right = mir_json_expression_graph_append(
            graph, "leaf", member_text, -1, -1, "none", "");
        if (right < 0)
            free(member_text);
        if (left < 0 || right < 0)
            return -1;
        kind = "member_access";
        break;
    }
    case AST_ARRAY_ACCESS:
        left = mir_json_expression_graph_build(
            graph, ast_array_access_array(expr));
        right = mir_json_expression_graph_build(
            graph, ast_array_access_index(expr));
        if (left < 0 || right < 0)
            return -1;
        kind = "index";
        break;
    case AST_CAST: {
        const char *target_type = ast_cast_target_type(expr);
        char *target_text;
        if (target_type == NULL || target_type[0] == '\0')
            return -1;
        left = mir_json_expression_graph_build(
            graph, ast_cast_operand(expr));
        target_text = mir_json_expression_graph_copy_text(target_type);
        right = mir_json_expression_graph_append(
            graph, "type_name", target_text, -1, -1, "none", "");
        if (right < 0)
            free(target_text);
        if (left < 0 || right < 0)
            return -1;
        kind = "cast";
        break;
    }
    default:
        return -1;
    }
    text = mir_json_expression_graph_capture_text(expr);
    if (text == NULL)
        return -1;
    left = mir_json_expression_graph_append(
        graph, kind, text, left, right, "none", "");
    if (left < 0)
        free(text);
    return left;
}

static ASTNode *
mir_json_instruction_expression(const MIRInstruction *inst, int lane)
{
    ASTNode *expr;

    if (inst == NULL || lane < 0 || lane > 1)
        return NULL;
    if (lane == 1) {
        if (inst->kind == MIR_INST_DEF
            && mir_instruction_source_node_type_or(inst, AST_PROGRAM)
                == AST_ASSIGNMENT) {
            return inst->expr1;
        }
        return NULL;
    }
    expr = inst->expr0;
    if (inst->kind != MIR_INST_STMT || expr == NULL
        || expr->type != AST_CALL || inst->arg0 == NULL) {
        return expr;
    }
    if (strcmp(inst->arg0, "Log") == 0)
        return ast_call_arg_count(expr) > 0
            ? ast_call_argument(expr, 0)
            : NULL;
    if (strcmp(inst->arg0, "ArrayPush") == 0)
        return ast_call_arg_count(expr) > 1
            ? ast_call_argument(expr, 1)
            : NULL;
    if (strcmp(inst->arg0, "ArraySet") == 0)
        return ast_call_arg_count(expr) > 2
            ? ast_call_argument(expr, 2)
            : NULL;
    return expr;
}

void
mir_json_emit_instruction_expression_graph(FILE *out,
                                           const MIRInstruction *inst,
                                           int lane)
{
    MIRJsonExpressionGraph graph = {0};
    ASTNode *expr = mir_json_instruction_expression(inst, lane);
    int root;

    if (out == NULL)
        return;
    root = mir_json_expression_graph_build(&graph, expr);
    if (root < 0) {
        fputs("null", out);
        mir_json_expression_graph_dispose(&graph);
        return;
    }
    fprintf(out, "{\"root\":%d,\"nodes\":[", root);
    for (size_t i = 0; i < graph.count; i++) {
        const MIRJsonExpressionGraphNode *node = &graph.nodes[i];
        if (i > 0)
            fputc(',', out);
        fputs("{\"kind\":", out);
        mir_json_emit_str(out, node->kind);
        fputs(",\"text\":", out);
        mir_json_emit_str(out, node->text);
        fputs(",\"call_target_kind\":", out);
        mir_json_emit_str(out, node->call_target_kind);
        fputs(",\"call_target_name\":", out);
        mir_json_emit_str(out, node->call_target_name);
        fputs(",\"left\":", out);
        if (node->left < 0)
            fputs("null", out);
        else
            fprintf(out, "%d", node->left);
        fputs(",\"right\":", out);
        if (node->right < 0)
            fputs("null", out);
        else
            fprintf(out, "%d", node->right);
        fputc('}', out);
    }
    fputs("]}", out);
    mir_json_expression_graph_dispose(&graph);
}
