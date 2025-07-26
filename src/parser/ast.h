#ifndef PERGYRA_AST_H
#define PERGYRA_AST_H

#include <stdint.h>
#include <stdbool.h>
#include "../lexer/lexer.h"

// AST 노드 타입
typedef enum {
    // 표현식
    AST_LITERAL,
    AST_IDENTIFIER,
    AST_FUNCTION_CALL,
    AST_MEMBER_ACCESS,
    AST_TYPE_PARAM,
    
    // 문장
    AST_LET_DECLARATION,
    AST_SLOT_CLAIM,
    AST_SLOT_WRITE,
    AST_SLOT_READ,
    AST_SLOT_RELEASE,
    AST_WITH_STATEMENT,
    AST_PARALLEL_BLOCK,
    AST_BLOCK,
    
    // 프로그램
    AST_PROGRAM
} ASTNodeType;

// 값 타입
typedef enum {
    VALUE_INT,
    VALUE_FLOAT,
    VALUE_STRING,
    VALUE_BOOL,
    VALUE_SLOT_REF
} ValueType;

// 값 구조체
typedef struct {
    ValueType type;
    union {
        int64_t int_val;
        double float_val;
        char* string_val;
        bool bool_val;
        struct {
            uint32_t slot_id;
            char* type_name;
        } slot_ref;
    } data;
} Value;

// AST 노드 (전방 선언)
typedef struct ASTNode ASTNode;

// 리터럴 노드
typedef struct {
    Value value;
} LiteralNode;

// 식별자 노드
typedef struct {
    char* name;
    char* type_name;  // 타입 추론 또는 명시적 타입
} IdentifierNode;

// 함수 호출 노드
typedef struct {
    char* function_name;
    ASTNode** arguments;
    size_t arg_count;
    ASTNode* type_params;  // 제네릭 타입 매개변수
} FunctionCallNode;

// 멤버 접근 노드
typedef struct {
    ASTNode* object;
    char* member_name;
} MemberAccessNode;

// 타입 매개변수 노드
typedef struct {
    char** type_names;
    size_t type_count;
} TypeParamNode;

// Let 선언 노드
typedef struct {
    char* variable_name;
    char* type_name;
    ASTNode* initializer;
} LetDeclarationNode;

// 슬롯 클레임 노드
typedef struct {
    char* type_name;
    char* slot_variable;
} SlotClaimNode;

// 슬롯 쓰기 노드
typedef struct {
    ASTNode* slot_ref;
    ASTNode* value;
} SlotWriteNode;

// 슬롯 읽기 노드
typedef struct {
    ASTNode* slot_ref;
} SlotReadNode;

// 슬롯 해제 노드
typedef struct {
    ASTNode* slot_ref;
} SlotReleaseNode;

// With 문 노드
typedef struct {
    char* type_name;
    char* slot_alias;
    ASTNode* body;
} WithStatementNode;

// 병렬 블록 노드
typedef struct {
    ASTNode** statements;
    size_t statement_count;
} ParallelBlockNode;

// 블록 노드
typedef struct {
    ASTNode** statements;
    size_t statement_count;
} BlockNode;

// 프로그램 노드
typedef struct {
    ASTNode** declarations;
    size_t declaration_count;
} ProgramNode;

// 메인 AST 노드 구조체
struct ASTNode {
    ASTNodeType type;
    uint32_t line;
    uint32_t column;
    
    union {
        LiteralNode literal;
        IdentifierNode identifier;
        FunctionCallNode function_call;
        MemberAccessNode member_access;
        TypeParamNode type_param;
        LetDeclarationNode let_declaration;
        SlotClaimNode slot_claim;
        SlotWriteNode slot_write;
        SlotReadNode slot_read;
        SlotReleaseNode slot_release;
        WithStatementNode with_statement;
        ParallelBlockNode parallel_block;
        BlockNode block;
        ProgramNode program;
    } data;
};

// AST 노드 생성 함수들
ASTNode* ast_create_literal(Value value, uint32_t line, uint32_t column);
ASTNode* ast_create_identifier(const char* name, uint32_t line, uint32_t column);
ASTNode* ast_create_function_call(const char* name, ASTNode** args, size_t arg_count, 
                                  uint32_t line, uint32_t column);
ASTNode* ast_create_member_access(ASTNode* object, const char* member, 
                                  uint32_t line, uint32_t column);
ASTNode* ast_create_let_declaration(const char* var_name, const char* type_name, 
                                    ASTNode* initializer, uint32_t line, uint32_t column);
ASTNode* ast_create_slot_claim(const char* type_name, const char* slot_var, 
                               uint32_t line, uint32_t column);
ASTNode* ast_create_slot_write(ASTNode* slot_ref, ASTNode* value, 
                               uint32_t line, uint32_t column);
ASTNode* ast_create_slot_read(ASTNode* slot_ref, uint32_t line, uint32_t column);
ASTNode* ast_create_slot_release(ASTNode* slot_ref, uint32_t line, uint32_t column);
ASTNode* ast_create_with_statement(const char* type_name, const char* alias, 
                                   ASTNode* body, uint32_t line, uint32_t column);
ASTNode* ast_create_parallel_block(ASTNode** statements, size_t count, 
                                   uint32_t line, uint32_t column);
ASTNode* ast_create_block(ASTNode** statements, size_t count, 
                          uint32_t line, uint32_t column);
ASTNode* ast_create_program(ASTNode** declarations, size_t count);

// AST 노드 해제
void ast_destroy_node(ASTNode* node);

// AST 출력 (디버깅용)
void ast_print_node(const ASTNode* node, int indent);
const char* ast_node_type_to_string(ASTNodeType type);

// 값 유틸리티
Value value_create_int(int64_t val);
Value value_create_float(double val);
Value value_create_string(const char* val);
Value value_create_bool(bool val);
void value_destroy(Value* value);
void value_print(const Value* value);

#endif // PERGYRA_AST_H