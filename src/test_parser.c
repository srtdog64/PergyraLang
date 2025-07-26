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

// 테스트 케이스 구조체
typedef struct {
    const char* name;
    const char* code;
} TestCase;

// 파서 테스트 실행
void test_parser(const char* name, const char* code) {
    printf("\n=== Test: %s ===\n", name);
    printf("Code:\n%s\n", code);
    printf("---\n");
    
    // 렉서 생성
    Lexer* lexer = lexer_create(code);
    if (!lexer) {
        printf("Failed to create lexer\n");
        return;
    }
    
    // 파서 생성
    Parser* parser = parser_create(lexer);
    if (!parser) {
        printf("Failed to create parser\n");
        lexer_destroy(lexer);
        return;
    }
    
    // 파싱
    ASTNode* ast = parser_parse_program(parser);
    
    if (parser_has_error(parser)) {
        printf("Parse error: %s\n", parser_get_error(parser));
    } else {
        printf("Parsing successful!\n\n");
        printf("AST:\n");
        ast_print(ast, 0);
    }
    
    // 정리
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
}

int main(void) {
    printf("=== Pergyra Parser Test ===\n");
    
    // 테스트 케이스들
    TestCase tests[] = {
        {
            "Basic Let Declaration",
            "let x = 42;\n"
            "let name = \"Pergyra\";\n"
            "let flag = true;"
        },
        {
            "Function Declaration",
            "func Add(a: Int, b: Int) -> Int {\n"
            "    return a + b;\n"
            "}"
        },
        {
            "Generic Function",
            "func Identity<T>(value: T) -> T {\n"
            "    return value;\n"
            "}"
        },
        {
            "Function with Where Clause",
            "func Sort<T>(items: Array<T>) -> Array<T>\n"
            "    where T: Comparable {\n"
            "    // Implementation\n"
            "    return items;\n"
            "}"
        },
        {
            "Slot Operations",
            "let slot = ClaimSlot<Int>();\n"
            "Write(slot, 42);\n"
            "let value = Read(slot);\n"
            "Release(slot);"
        },
        {
            "With Statement",
            "with slot<String> as s {\n"
            "    s.Write(\"Hello\");\n"
            "    Log(s.Read());\n"
            "}"
        },
        {
            "Secure Slot",
            "with SecureSlot<Int>(SECURITY_LEVEL_HARDWARE) as hp {\n"
            "    hp.Write(100);\n"
            "}"
        },
        {
            "Parallel Block",
            "let result = Parallel {\n"
            "    ProcessA();\n"
            "    ProcessB();\n"
            "    ProcessC();\n"
            "};"
        },
        {
            "For Loop",
            "for i in 1..10 {\n"
            "    Log(i);\n"
            "}"
        },
        {
            "If Statement",
            "if x > 10 {\n"
            "    Log(\"Greater\");\n"
            "} else {\n"
            "    Log(\"Less or equal\");\n"
            "}"
        },
        {
            "Class Declaration",
            "class Player<T> where T: Serializable {\n"
            "    private let _name: String;\n"
            "    public let Health: Int;\n"
            "    \n"
            "    public func TakeDamage(amount: Int) {\n"
            "        Health = Health - amount;\n"
            "    }\n"
            "}"
        },
        {
            "Complex Expression",
            "let result = (a + b * c) / (d - e) && flag || !other;"
        },
        {
            "Method Chaining",
            "let result = object.Method1().Method2(42).Property;"
        },
        {
            "Array Access",
            "let value = array[index + 1];\n"
            "matrix[i][j] = value * 2;"
        },
        {
            "Full Example",
            "// Fibonacci function\n"
            "func Fibonacci(n: Int) -> Int {\n"
            "    if n <= 1 {\n"
            "        return n;\n"
            "    }\n"
            "    \n"
            "    with slot<Int> as prev {\n"
            "        prev.Write(0);\n"
            "        \n"
            "        with slot<Int> as curr {\n"
            "            curr.Write(1);\n"
            "            \n"
            "            for i in 2..n {\n"
            "                let next = prev.Read() + curr.Read();\n"
            "                prev.Write(curr.Read());\n"
            "                curr.Write(next);\n"
            "            }\n"
            "            \n"
            "            return curr.Read();\n"
            "        }\n"
            "    }\n"
            "}"
        }
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    
    for (int i = 0; i < num_tests; i++) {
        test_parser(tests[i].name, tests[i].code);
        printf("\n");
    }
    
    printf("\n=== All tests completed ===\n");
    
    return 0;
}
