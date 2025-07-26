/*
 * Copyright (c) 2025 Pergyra Language Project
 * Party System Compiler Support
 * Generates FiberMap metadata at compile time
 */

#ifndef PERGYRA_PARTY_COMPILER_H
#define PERGYRA_PARTY_COMPILER_H

#include "../parser/ast.h"
#include "party_runtime.h"

/* ============= Compile-time Analysis ============= */

/* Party type information extracted from AST */
typedef struct {
    const char* partyName;
    
    /* Role slots */
    struct {
        const char* slotName;
        const char** requiredAbilities;
        size_t abilityCount;
        bool isArray;
    }* roleSlots;
    size_t roleSlotCount;
    
    /* Shared fields */
    struct {
        const char* fieldName;
        const char* typeName;
        ASTNode* initializer;
    }* sharedFields;
    size_t sharedFieldCount;
    
    /* Methods */
    ASTNode** methods;
    size_t methodCount;
    
    /* Parent party (for inheritance) */
    const char* extendsParty;
    
    /* Generic parameters */
    GenericParams* genericParams;
} PartyTypeInfo;

/* Role implementation info */
typedef struct {
    const char* roleName;
    const char* forType;
    
    /* Parallel block info */
    struct {
        bool hasParallelBlock;
        ASTNode* parallelBlock;
        const char* schedulerName;
        int priority;
        uint32_t intervalMs;
        bool isContinuous;
    } parallelInfo;
    
    /* Implemented abilities */
    const char** implementedAbilities;
    size_t abilityCount;
} RoleImplInfo;

/* ============= Analysis Functions ============= */

/* Extract party type info from AST */
PartyTypeInfo* AnalyzePartyDeclaration(ASTNode* partyDecl);

/* Extract role implementation info */
RoleImplInfo* AnalyzeRoleDeclaration(ASTNode* roleDecl);

/* Match role implementations to party slots */
typedef struct {
    const char* slotName;
    RoleImplInfo* roleImpl;
    const char* concreteType;  /* After generic instantiation */
} RoleSlotBinding;

/* Validate party instance creation */
typedef struct {
    bool isValid;
    const char* error;
    RoleSlotBinding* bindings;
    size_t bindingCount;
} PartyValidationResult;

PartyValidationResult ValidatePartyInstance(
    PartyTypeInfo* partyType,
    ASTNode* partyInstance,
    RoleImplInfo** availableRoles,
    size_t roleCount
);

/* ============= Code Generation ============= */

/* Generate static FiberMap initialization code */
typedef struct {
    char* code;              /* Generated C code */
    size_t codeLength;
    const char* symbolName;  /* Symbol name for the fiber map */
} GeneratedFiberMap;

GeneratedFiberMap GenerateStaticFiberMap(
    PartyTypeInfo* partyType,
    RoleSlotBinding* bindings,
    size_t bindingCount
);

/* Generate party context initialization */
char* GeneratePartyContextInit(
    PartyTypeInfo* partyType,
    const char* instanceName
);

/* Generate parallel dispatch call */
char* GenerateParallelDispatch(
    const char* partyInstanceName,
    const char* fiberMapSymbol,
    JoinStrategy joinStrategy
);

/* ============= Metadata Tables ============= */

/* Generate compile-time metadata tables */
typedef struct {
    /* Role parallel metadata table */
    char* parallelMetadataTable;
    
    /* Ability implementation table */
    char* abilityImplTable;
    
    /* Party type registry */
    char* partyTypeRegistry;
} MetadataTables;

MetadataTables GenerateMetadataTables(
    PartyTypeInfo** parties,
    size_t partyCount,
    RoleImplInfo** roles,
    size_t roleCount
);

/* ============= Optimization ============= */

/* Analyze party for static optimization opportunities */
typedef struct {
    bool canStaticDispatch;      /* All bindings known at compile time */
    bool canInlineRoles;         /* Small roles can be inlined */
    bool canCacheFiberMap;       /* FiberMap is immutable */
    bool canParallelizeInit;     /* Initialization can be parallel */
    
    /* Scheduler hints */
    uint32_t estimatedCpuFibers;
    uint32_t estimatedGpuFibers;
    uint32_t estimatedIoFibers;
    
    /* Memory estimates */
    size_t estimatedStackSize;
    size_t estimatedHeapSize;
} PartyOptimizationHints;

PartyOptimizationHints AnalyzePartyOptimizations(
    PartyTypeInfo* partyType,
    RoleSlotBinding* bindings,
    size_t bindingCount
);

/* ============= Error Reporting ============= */

/* Compile-time error types */
typedef enum {
    PARTY_ERROR_NONE,
    PARTY_ERROR_MISSING_SLOT,
    PARTY_ERROR_ABILITY_MISMATCH,
    PARTY_ERROR_CIRCULAR_DEPENDENCY,
    PARTY_ERROR_INVALID_SCHEDULER,
    PARTY_ERROR_GENERIC_MISMATCH,
    PARTY_ERROR_INHERITANCE_CONFLICT
} PartyErrorType;

typedef struct {
    PartyErrorType type;
    const char* message;
    const char* partyName;
    const char* slotName;
    uint32_t line;
    uint32_t column;
} PartyCompileError;

/* Check for party system errors */
PartyCompileError* CheckPartyErrors(
    PartyTypeInfo** parties,
    size_t partyCount,
    size_t* errorCount
);

/* ============= Debug Support ============= */

/* Generate party visualization (GraphViz) */
char* GeneratePartyVisualization(
    PartyTypeInfo* partyType,
    RoleSlotBinding* bindings,
    size_t bindingCount
);

/* Generate runtime inspection code */
char* GeneratePartyInspector(
    PartyTypeInfo* partyType,
    const char* instanceName
);

/* ============= Integration with Parser ============= */

/* Hook into parser to handle party declarations */
void RegisterPartyParserHandlers(void);

/* Transform party AST nodes to IR */
typedef struct PartyIR PartyIR;

PartyIR* TransformPartyToIR(
    ASTNode* partyDecl,
    struct SymbolTable* symbols
);

/* ============= C Code Templates ============= */

/* Template for static fiber map */
#define FIBER_MAP_TEMPLATE \
    "static const FiberMapEntry %s_entries[] = {\n" \
    "%s" \
    "};\n\n" \
    "static const FiberMap %s = {\n" \
    "    .partyTypeName = \"%s\",\n" \
    "    .entries = %s_entries,\n" \
    "    .entryCount = %zu,\n" \
    "    .cacheKey = 0x%llx,\n" \
    "    .isStatic = true\n" \
    "};\n"

/* Template for role metadata */
#define ROLE_METADATA_TEMPLATE \
    "static const RoleParallelMetadata %s_metadata = {\n" \
    "    .roleName = \"%s\",\n" \
    "    .function = (ParallelFunction)%s,\n" \
    "    .scheduler = %s,\n" \
    "    .priority = %d,\n" \
    "    .intervalMs = %u,\n" \
    "    .continuous = %s\n" \
    "};\n"

/* Template for party context */
#define PARTY_CONTEXT_TEMPLATE \
    "static PartyContext %s_context = {\n" \
    "    .roles = %s_roles,\n" \
    "    .roleCount = %zu,\n" \
    "    .sharedFields = %s_shared,\n" \
    "    .sharedFieldCount = %zu,\n" \
    "    .partyName = \"%s\",\n" \
    "    .inCombat = false,\n" \
    "    .contextLock = SPINLOCK_INIT\n" \
    "};\n"

#endif /* PERGYRA_PARTY_COMPILER_H */
