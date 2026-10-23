#include "operator_overload_rules.h"

#include <string.h>

typedef struct OperatorOverloadRule {
    PgyTokenType op;
    const char *suffix;
    const char *names[9];
} OperatorOverloadRule;

/* Enumeration, free-function spelling and role aliases are one language rule.
 * Keep target selection and target-specific emission with their consumers. */
static const OperatorOverloadRule rules[] = {
    { TOKEN_PLUS, "add", { "Add", "add", "OperatorAdd", "operator_add", NULL } },
    { TOKEN_MINUS, "sub", { "Sub", "sub", "Subtract", "subtract",
                            "OperatorSub", "operator_sub", NULL } },
    { TOKEN_STAR, "mul", { "Mul", "mul", "Multiply", "multiply",
                           "OperatorMul", "operator_mul", NULL } },
    { TOKEN_SLASH, "div", { "Div", "div", "Divide", "divide",
                            "OperatorDiv", "operator_div", NULL } },
    { TOKEN_PERCENT, "mod", { "Mod", "mod", "Modulo", "modulo",
                              "OperatorMod", "operator_mod", NULL } },
    { TOKEN_EQUAL, "eq", { "Eq", "eq", "Equal", "equal", "Equals", "equals",
                          "OperatorEq", "operator_eq", NULL } },
    { TOKEN_NOT_EQUAL, "ne", { "Ne", "ne", "NotEqual", "notEqual",
                              "NotEquals", "notEquals", "OperatorNe",
                              "operator_ne", NULL } },
    { TOKEN_LESS, "lt", { "Lt", "lt", "LessThan", "lessThan",
                         "OperatorLt", "operator_lt", NULL } },
    { TOKEN_LESS_EQUAL, "le", { "Le", "le", "LessEqual", "lessEqual",
                               "LessThanOrEqual", "lessThanOrEqual",
                               "OperatorLe", "operator_le", NULL } },
    { TOKEN_GREATER, "gt", { "Gt", "gt", "GreaterThan", "greaterThan",
                            "OperatorGt", "operator_gt", NULL } },
    { TOKEN_GREATER_EQUAL, "ge", { "Ge", "ge", "GreaterEqual", "greaterEqual",
                                  "GreaterThanOrEqual", "greaterThanOrEqual",
                                  "OperatorGe", "operator_ge", NULL } },
};

size_t
pgy_operator_overload_count(void)
{
    return sizeof(rules) / sizeof(rules[0]);
}

PgyTokenType
pgy_operator_overload_at(size_t index)
{
    return index < pgy_operator_overload_count() ? rules[index].op : TOKEN_EOF;
}

static const OperatorOverloadRule *
operator_overload_rule(PgyTokenType op)
{
    for (size_t i = 0; i < pgy_operator_overload_count(); i++) {
        if (rules[i].op == op)
            return &rules[i];
    }
    return NULL;
}

const char *
pgy_operator_overload_suffix(PgyTokenType op)
{
    const OperatorOverloadRule *rule = operator_overload_rule(op);
    return rule != NULL ? rule->suffix : NULL;
}

bool
pgy_operator_method_name_matches(PgyTokenType op, const char *name)
{
    const OperatorOverloadRule *rule = operator_overload_rule(op);
    if (rule == NULL || name == NULL)
        return false;
    for (size_t i = 0; rule->names[i] != NULL; i++) {
        if (strcmp(rule->names[i], name) == 0)
            return true;
    }
    return false;
}
