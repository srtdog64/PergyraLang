/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PERGYRA_SLOT_ANALYZER_INTERNAL_H
#define PERGYRA_SLOT_ANALYZER_INTERNAL_H

#include "slot_analyzer.h"

enum {
    SLOT_ACCESS_READ = 1u << 0,
    SLOT_ACCESS_WRITE = 1u << 1,
    SLOT_ACCESS_RELEASE = 1u << 2
};

typedef struct
{
    const char *name;
    unsigned    mask;
} SlotAccessEntry;

typedef struct
{
    const char *name;
    unsigned    mask;
} SlotEscapeEntry;

typedef struct
{
    ASTNode    *function_decl;
    size_t      param_index;
    const char *param_name;
} SlotSummaryOrigin;

typedef struct
{
    SemanticContext *ctx;
    ASTNode         *program_root;
} SlotFunctionLookup;

unsigned slot_builtin_access_mask(const char *name);
bool slot_builtin_call_is_local_non_escape(const char *name);

void slot_access_record(SlotAccessEntry **entries, size_t *count,
                        size_t *capacity, const char *name, unsigned mask);
void slot_escape_record(SlotEscapeEntry **entries, size_t *count,
                        size_t *capacity, const char *name, unsigned mask);

ASTNode *slot_analyzer_find_function_decl(const SlotFunctionLookup *lookup,
                                          const char *name);
unsigned function_param_flow_summary_demand(
    const SlotFunctionLookup *lookup,
    ASTNode *function_decl,
    size_t param_index);
void function_param_flow_summary_store_destroy(SemanticContext *ctx);
unsigned slot_access_mask_for_named_symbol(ASTNode *node,
                                           const char *symbol_name,
                                           const SlotFunctionLookup *lookup,
                                           int depth);
unsigned slot_escape_mask_in_program(ASTNode *node, const char *slot_name,
                                     const SlotFunctionLookup *lookup,
                                     int depth,
                                     const SlotSummaryOrigin *origin);
unsigned slot_param_summary_in_program(ASTNode *node, const char *slot_name,
                                       const SlotFunctionLookup *lookup,
                                       int depth,
                                       const SlotSummaryOrigin *origin);
void collect_slot_escapes(ASTNode *node, SlotEscapeEntry **entries,
                          size_t *count, size_t *capacity,
                          const SlotFunctionLookup *lookup, int depth,
                          const SlotSummaryOrigin *origin);
void collect_slot_accesses(ASTNode *node, SlotAccessEntry **entries,
                           size_t *count, size_t *capacity,
                           const SlotFunctionLookup *lookup);

#endif /* PERGYRA_SLOT_ANALYZER_INTERNAL_H */
