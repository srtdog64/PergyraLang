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

void slot_access_record(SlotAccessEntry **entries, size_t *count,
                        size_t *capacity, const char *name, unsigned mask);
void slot_escape_record(SlotEscapeEntry **entries, size_t *count,
                        size_t *capacity, const char *name, unsigned mask);

ASTNode *slot_analyzer_find_function_decl(ASTNode *program, const char *name);
unsigned slot_access_mask_for_named_symbol(ASTNode *node,
                                           const char *symbol_name,
                                           ASTNode *program_root, int depth);
unsigned slot_escape_mask_in_program(ASTNode *node, const char *slot_name,
                                     ASTNode *program_root, int depth);
unsigned slot_param_summary_in_program(ASTNode *node, const char *slot_name,
                                       ASTNode *program_root, int depth);
void collect_slot_escapes(ASTNode *node, SlotEscapeEntry **entries,
                          size_t *count, size_t *capacity,
                          ASTNode *program_root, int depth);
void collect_slot_accesses(ASTNode *node, SlotAccessEntry **entries,
                           size_t *count, size_t *capacity,
                           ASTNode *program_root);

#endif /* PERGYRA_SLOT_ANALYZER_INTERNAL_H */
