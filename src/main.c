#include "lexer/lexer.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    const char* test_code = 
        "// Pergyra 언어 테스트\n"
        "let slot = claim_slot<Int>()\n"
        "write(slot, 42)\n"
        "let val = read(slot)\n"
        "release(slot)\n"
        "\n"
        "with slot<String> as s {\n"
        "    s.write(\"hello world\")\n"
        "    log(s.read())\n"
        "}\n"
        "\n"
        "let result = parallel {\n"
        "    process_a()\n"
        "    process_b()\n"
        "    process_c()\n"
        "}\n";
    
    printf("=== Pergyra 토크나이저 테스트 ===\n");
    printf("소스 코드:\n%s\n", test_code);
    printf("\n=== 토큰 분석 결과 ===\n");
    
    Lexer* lexer = lexer_create(test_code);
    if (!lexer) {
        fprintf(stderr, "렉서 생성 실패\n");
        return 1;
    }
    
    Token token;
    int token_count = 0;
    
    do {
        token = lexer_next_token(lexer);
        
        if (token.type == TOKEN_ERROR) {
            printf("ERROR: %s (line %u, col %u)\n", 
                   lexer_get_error(lexer), token.line, token.column);
            break;
        }
        
        if (token.type != TOKEN_NEWLINE) { // 개행은 출력하지 않음
            printf("%3d: ", ++token_count);
            token_print(&token);
        }
        
    } while (token.type != TOKEN_EOF && token.type != TOKEN_ERROR);
    
    if (lexer_has_error(lexer)) {
        printf("\n렉서 에러 발생: %s\n", lexer_get_error(lexer));
    } else {
        printf("\n총 %d개의 토큰이 성공적으로 분석되었습니다.\n", token_count);
    }
    
    lexer_destroy(lexer);
    return 0;
}