/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Pergyra Language Project nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "lexer/lexer.h"
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
    const char *testCode = 
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
    
    Lexer *lexer;
    Token  token;
    int    tokenCount = 0;
    
    printf("=== Pergyra 토크나이저 테스트 ===\n");
    printf("소스 코드:\n%s\n", testCode);
    printf("\n=== 토큰 분석 결과 ===\n");
    
    lexer = LexerCreate(testCode);
    if (lexer == NULL) {
        fprintf(stderr, "렉서 생성 실패\n");
        return 1;
    }
    
    do {
        token = LexerNextToken(lexer);
        
        if (token.type == TOKEN_ERROR) {
            printf("ERROR: %s (line %u, col %u)\n", 
                   LexerGetError(lexer), token.line, token.column);
            break;
        }
        
        if (token.type != TOKEN_NEWLINE) { /* Skip newlines in output */
            printf("%3d: ", ++tokenCount);
            TokenPrint(&token);
        }
        
    } while (token.type != TOKEN_EOF && token.type != TOKEN_ERROR);
    
    if (LexerHasError(lexer)) {
        printf("\n렉서 에러 발생: %s\n", LexerGetError(lexer));
    } else {
        printf("\n총 %d개의 토큰이 성공적으로 분석되었습니다.\n", tokenCount);
    }
    
    LexerDestroy(lexer);
    return 0;
}