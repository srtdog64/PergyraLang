#include "semantic/operator_overload_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

#define CHECK(test) do { \
    checks++; \
    if (!(test)) { \
        fprintf(stderr, "operator rules: line %d: %s\n", __LINE__, #test); \
        exit(1); \
    } \
} while (0)

/* Independent expected language behavior, not a projection of the owner. */
static const struct {
    PgyTokenType op;
    const char *suffix;
    const char *names;
} expected[] = {
    { TOKEN_PLUS, "add", "Add add OperatorAdd operator_add" },
    { TOKEN_MINUS, "sub", "Sub sub Subtract subtract OperatorSub operator_sub" },
    { TOKEN_STAR, "mul", "Mul mul Multiply multiply OperatorMul operator_mul" },
    { TOKEN_SLASH, "div", "Div div Divide divide OperatorDiv operator_div" },
    { TOKEN_PERCENT, "mod", "Mod mod Modulo modulo OperatorMod operator_mod" },
    { TOKEN_EQUAL, "eq", "Eq eq Equal equal Equals equals OperatorEq operator_eq" },
    { TOKEN_NOT_EQUAL, "ne", "Ne ne NotEqual notEqual NotEquals notEquals OperatorNe operator_ne" },
    { TOKEN_LESS, "lt", "Lt lt LessThan lessThan OperatorLt operator_lt" },
    { TOKEN_LESS_EQUAL, "le", "Le le LessEqual lessEqual LessThanOrEqual lessThanOrEqual OperatorLe operator_le" },
    { TOKEN_GREATER, "gt", "Gt gt GreaterThan greaterThan OperatorGt operator_gt" },
    { TOKEN_GREATER_EQUAL, "ge", "Ge ge GreaterEqual greaterEqual GreaterThanOrEqual greaterThanOrEqual OperatorGe operator_ge" },
};

int
main(void)
{
    size_t count = sizeof(expected) / sizeof(expected[0]);
    CHECK(pgy_operator_overload_count() == count);
    CHECK(pgy_operator_overload_at(count) == TOKEN_EOF);
    CHECK(pgy_operator_overload_at(SIZE_MAX) == TOKEN_EOF);
    CHECK(pgy_operator_overload_suffix((PgyTokenType)-1) == NULL);
    CHECK(pgy_operator_overload_suffix(TOKEN_NEWLINE) == NULL);
    CHECK(pgy_operator_overload_suffix(TOKEN_ERROR) == NULL);

    for (size_t i = 0; i < count; i++) {
        CHECK(pgy_operator_overload_at(i) == expected[i].op);
        const char *suffix = pgy_operator_overload_suffix(expected[i].op);
        CHECK(suffix != NULL && strcmp(suffix, expected[i].suffix) == 0);
        char names[160];
        snprintf(names, sizeof(names), "%s", expected[i].names);
        for (char *name = strtok(names, " "); name != NULL;
             name = strtok(NULL, " ")) {
            char near_name[180];
            snprintf(near_name, sizeof(near_name), "%sExtra", name);
            CHECK(!pgy_operator_method_name_matches(expected[i].op, near_name));
            snprintf(near_name, sizeof(near_name), "_%s", name);
            CHECK(!pgy_operator_method_name_matches(expected[i].op, near_name));
            CHECK(!pgy_operator_method_name_matches((PgyTokenType)-1, name));
            CHECK(!pgy_operator_method_name_matches(TOKEN_ERROR, name));
            /* Test every preceding lexer token, not only overload tokens. */
            for (int token = 0; token <= TOKEN_EOF; token++)
                CHECK(pgy_operator_method_name_matches((PgyTokenType)token, name)
                    == ((PgyTokenType)token == expected[i].op));
        }
    }
    for (int token = 0; token <= TOKEN_EOF; token++) {
        const char *want = NULL;
        for (size_t i = 0; i < count; i++) {
            if (expected[i].op == (PgyTokenType)token)
                want = expected[i].suffix;
        }
        const char *got = pgy_operator_overload_suffix((PgyTokenType)token);
        CHECK((want == NULL && got == NULL)
            || (want != NULL && got != NULL && strcmp(want, got) == 0));
        CHECK(!pgy_operator_method_name_matches((PgyTokenType)token, NULL));
        CHECK(!pgy_operator_method_name_matches((PgyTokenType)token, ""));
        CHECK(!pgy_operator_method_name_matches((PgyTokenType)token, "ADD"));
    }
    printf("[operator-overload-rules] %u checks PASS\n", checks);
    return 0;
}
