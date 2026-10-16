#ifndef STRING_WINDOW_EXTENT_INTERNAL_H
#define STRING_WINDOW_EXTENT_INTERNAL_H

#include "string_window_extent.h"
#include "type_checker_internal.h"

typedef struct
{
    size_t source;
    size_t extent;
} SweRequirement;

typedef struct
{
    const char *source;
    const char *extent;
} SweFieldPair;

typedef struct
{
    uint32_t decl_id;
    ASTNode *node;
    bool is_method;
    SweRequirement *requirements;
    size_t requirement_count;
    size_t requirement_capacity;
} SweRoutine;

typedef struct
{
    ASTNode *call;
    ASTNode *routine;
} SweCall;

typedef struct
{
    ASTNode *call;
    ASTNode *routine;
    ASTNode *decl;
} SweConstructor;

typedef struct
{
    uint32_t decl_id;
    ASTNode *node;
} SweLet;

typedef struct
{
    ASTNode *identifier;
    uint32_t decl_id;
} SweFunctionValue;

typedef struct
{
    const char *name;
    ASTNode *owner;   /* the record declaring the field; NULL when unknown */
    ASTNode *site;
} SweFieldWrite;

struct StringWindowExtentStore
{
    SweRoutine *routines;
    size_t routine_count;
    size_t routine_capacity;
    SweCall *calls;
    size_t call_count;
    size_t call_capacity;
    SweConstructor *constructors;
    size_t constructor_count;
    size_t constructor_capacity;
    SweLet *lets;
    size_t let_count;
    size_t let_capacity;
    uint32_t *mutated;
    size_t mutated_count;
    size_t mutated_capacity;
    uint32_t *inout_passed;
    size_t inout_passed_count;
    size_t inout_passed_capacity;
    uint32_t *parameters;
    size_t parameter_count;
    size_t parameter_capacity;
    SweFieldWrite *field_writes;
    size_t field_write_count;
    size_t field_write_capacity;
    SweFieldPair *field_requirements;
    size_t field_requirement_count;
    size_t field_requirement_capacity;
    SweFunctionValue *function_values;
    size_t function_value_count;
    size_t function_value_capacity;
    bool allocation_failed;
};

typedef enum
{
    SWE_UNPROVEN = 0,
    SWE_PROVEN,
    SWE_REQUIREMENT,
    SWE_FIELD_REQUIREMENT
} SweVerdict;

bool
swe_reserve(void **items, size_t *capacity, size_t count, size_t item_size);

StringWindowExtentStore *
swe_store(SemanticContext *ctx);

void
swe_push_id(StringWindowExtentStore *store, uint32_t **items, size_t *count,
            size_t *capacity, uint32_t decl_id);

ASTNode *
swe_root_identifier(ASTNode *node);

void
swe_mark_place_fields(SemanticContext *ctx, StringWindowExtentStore *store,
                      ASTNode *place, ASTNode *site);

int
swe_compare_u32(const void *a, const void *b);

int
swe_compare_routine(const void *a, const void *b);

int
swe_compare_let(const void *a, const void *b);

SweRoutine *
swe_find_routine(StringWindowExtentStore *store, uint32_t decl_id);

SweRoutine *
swe_find_routine_node(StringWindowExtentStore *store, const ASTNode *node);

const char *
swe_bare_builtin_name(const ASTNode *call);

bool
swe_is_window_call(const ASTNode *call);

SweVerdict
swe_classify(StringWindowExtentStore *store, const ASTNode *source,
             const ASTNode *extent, const SweRoutine *routine,
             SweRequirement *requirement_out, SweFieldPair *field_out);

bool
swe_apply_verdict(StringWindowExtentStore *store, SweRoutine *caller,
                  SweVerdict verdict, SweRequirement requirement,
                  SweFieldPair field, bool *unproven);

#endif
