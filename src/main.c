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
    (void)argc;
    (void)argv;

    const char *testCode =
        "// Pergyra language smoke test\n"
        "let slot = ClaimSlot<Int>();\n"
        "Write(slot, 42);\n"
        "let val = Read(slot);\n"
        "Release(slot);\n"
        "\n"
        "with slot<String> as s {\n"
        "    s.Write(\"hello world\");\n"
        "    Log(s.Read());\n"
        "}\n"
        "\n"
        "let result = parallel {\n"
        "    ProcessA();\n"
        "    ProcessB();\n"
        "    ProcessC();\n"
        "};\n";
    
    Lexer *lexer;
    Token  token;
    int    tokenCount = 0;
    
    printf("=== Pergyra lexer smoke test ===\n");
    printf("Source code:\n%s\n", testCode);
    printf("\n=== Token analysis result ===\n");
    
    lexer = lexer_create(testCode);
    if (lexer == NULL) {
        fprintf(stderr, "Failed to create lexer\n");
        return 1;
    }
    
    do {
        token = lexer_next_token(lexer);
        
        if (token.type == TOKEN_ERROR) {
            printf("ERROR: %s (line %u, col %u)\n", 
                   lexer_get_error(lexer), token.line, token.column);
            break;
        }

        printf("%3d: ", ++tokenCount);
        token_print(&token);
        
    } while (token.type != TOKEN_EOF && token.type != TOKEN_ERROR);
    
    if (lexer_has_error(lexer)) {
        printf("\nLexer error: %s\n", lexer_get_error(lexer));
    } else {
        printf("\nSuccessfully analyzed %d tokens.\n", tokenCount);
    }
    
    lexer_destroy(lexer);
    return 0;
}
