/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parser test driver
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "parser/ast.h"

typedef struct {
    const char *name;
    const char *code;
    int expect_success;
} TestCase;

static int
run_parser_test(const TestCase *test)
{
    int failed = 0;
    Lexer *lexer;
    Parser *parser;
    ASTNode *ast;

    printf("\n=== Test: %s ===\n", test->name);
    printf("Code:\n%s\n", test->code);
    printf("---\n");

    lexer = lexer_create(test->code);
    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("Parse error: %s\n", parser_get_error(parser));
        if (test->expect_success) {
            printf("[FAIL] unexpected parse error\n");
            failed = 1;
        }
    } else {
        printf("Parsing successful!\n\n");
        printf("AST:\n");
        ast_print(ast, 0);
        if (!test->expect_success) {
            printf("[FAIL] expected parse failure but succeeded\n");
            failed = 1;
        }
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

int
main(void)
{
    const TestCase tests[] = {
        {
            "Basic Let Declaration",
            "let x = 42;\n"
            "let name = \"Pergyra\";\n"
            "let flag = true;",
            1
        },
        {
            "Function Declaration",
            "func Add(a: Int, b: Int) -> Int {\n"
            "    return a + b;\n"
            "}",
            1
        },
        {
            "Generic Function",
            "func Identity<T>(value: T) -> T {\n"
            "    return value;\n"
            "}",
            1
        },
        {
            "Function with Where Clause",
            "func Sort<T>(items: Array<T>) -> Array<T>\n"
            "    where T: Comparable {\n"
            "    // Implementation\n"
            "    return items;\n"
            "}",
            1
        },
        {
            "Slot Operations",
            "let slot = ClaimSlot<Int>();\n"
            "Write(slot, 42);\n"
            "let value = Read(slot);\n"
            "Release(slot);",
            1
        },
        {
            "With Statement",
            "with slot<String> as s {\n"
            "    s.Write(\"Hello\");\n"
            "    Log(s.Read());\n"
            "}",
            1
        },
        {
            "Secure Slot",
            "with SecureSlot<Int>(SECURITY_LEVEL_HARDWARE) as hp {\n"
            "    hp.Write(100);\n"
            "}",
            1
        },
        {
            "Parallel Block",
            "let result = parallel {\n"
            "    ProcessA();\n"
            "    ProcessB();\n"
            "    ProcessC();\n"
            "};",
            1
        },
        {
            "For Loop",
            "for i in 1..10 {\n"
            "    Log(i);\n"
            "}",
            1
        },
        {
            "If Statement",
            "if x > 10 {\n"
            "    Log(\"Greater\");\n"
            "} else {\n"
            "    Log(\"Less or equal\");\n"
            "}",
            1
        },
        {
            "Class Declaration",
            "class Player<T> where T: Serializable {\n"
            "    private let _name: String;\n"
            "    public let Health: Int;\n"
            "    public func TakeDamage(amount: Int) {\n"
            "        Health = Health - amount;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Struct Declaration",
            "struct Vec3 {\n"
            "    x: Float;\n"
            "    y: Float;\n"
            "    z: Float;\n"
            "    func Length() -> Float {\n"
            "        return x;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Extern C Block",
            "extern \"C\" {\n"
            "    func SDL_Init(flags: Int) -> Int;\n"
            "    func SDL_Quit();\n"
            "}\n"
            "func Main() -> Int {\n"
            "    return SDL_Init(0);\n"
            "}",
            1
        },
        {
            "Complex Expression",
            "let result = (a + b * c) / (d - e) && flag || !other;",
            1
        },
        {
            "Method Chaining",
            "let result = object.Method1().Method2(42).Property;",
            1
        },
        {
            "Array Access",
            "let value = array[index + 1];\n"
            "matrix[i][j] = value * 2;",
            1
        },
        {
            "While Loop",
            "func Countdown(n: Int) -> Void {\n"
            "    let count: Int = n;\n"
            "    while count > 0 {\n"
            "        Log(count);\n"
            "        count = count - 1;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Match Statement",
            "func Classify(n: Int) -> Void {\n"
            "    match n {\n"
            "        case 0:\n"
            "            Log(\"zero\");\n"
            "        case 1:\n"
            "            Log(\"one\");\n"
            "        case 2 if n > 0:\n"
            "            Log(\"two positive\");\n"
            "        default:\n"
            "            Log(\"other\");\n"
            "    }\n"
            "}",
            1
        },
        {
            "Full Example",
            "// Fibonacci function\n"
            "func Fibonacci(n: Int) -> Int {\n"
            "    if n <= 1 {\n"
            "        return n;\n"
            "    }\n"
            "    with slot<Int> as prev {\n"
            "        prev.Write(0);\n"
            "        with slot<Int> as curr {\n"
            "            curr.Write(1);\n"
            "            for i in 2..n {\n"
            "                let next = prev.Read() + curr.Read();\n"
            "                prev.Write(curr.Read());\n"
            "                curr.Write(next);\n"
            "            }\n"
            "            return curr.Read();\n"
            "        }\n"
            "    }\n"
            "}",
            1
        },
        {
            "Ability Declaration",
            "ability Damageable {\n"
            "    require health: Int\n"
            "    func TakeDamage(amount: Int) -> Void {\n"
            "        Log(amount);\n"
            "    }\n"
            "    func GetHealth() -> Int;\n"
            "}",
            1
        },
        {
            "Role Declaration",
            "role PlayerDamageable for Player {\n"
            "    include role BuffableRole\n"
            "    impl ability Damageable {\n"
            "        func TakeDamage(amount: Int) -> Void {\n"
            "            Log(amount);\n"
            "        }\n"
            "    }\n"
            "    override func GetHealth() -> Int {\n"
            "        return 100;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Party Declaration",
            "party DungeonTeam {\n"
            "    role slot tank: Damageable\n"
            "    role slot healer: Healing\n"
            "    shared formation: String = \"standard\"\n"
            "    func Execute() -> Void {\n"
            "        Log(formation);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Systemic Declaration",
            "systemic CombatSystem {\n"
            "    party slot team1: DungeonTeam\n"
            "    party slot team2: DungeonTeam\n"
            "    shared round: Int = 0\n"
            "    func StartRound() -> Void {\n"
            "        Log(round);\n"
            "    }\n"
            "}",
            1
        },
        {
            "World Declaration",
            "world GameWorld {\n"
            "    systemic combat: CombatSystem\n"
            "    shared tick: Int = 0\n"
            "    func Update() -> Void {\n"
            "        Log(tick);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Actor Declaration",
            "actor Counter {\n"
            "    let count: Int;\n"
            "    func Increment() -> Void {\n"
            "        count = count + 1;\n"
            "    }\n"
            "}",
            1
        }
    };

    int failures = 0;
    size_t num_tests = sizeof(tests) / sizeof(tests[0]);

    printf("=== Pergyra Parser Test ===\n");

    for (size_t i = 0; i < num_tests; i++) {
        failures += run_parser_test(&tests[i]);
        printf("\n");
    }

    printf("\n=== All tests completed ===\n");
    if (failures > 0) {
        printf("Parser test failures: %d\n", failures);
        return 1;
    }

    return 0;
}
