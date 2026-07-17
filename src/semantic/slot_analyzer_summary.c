/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot analyzer parameter summary composition.
 */

#include "slot_analyzer_internal.h"

unsigned
slot_param_summary_in_program(ASTNode *node, const char *slot_name,
                              const SlotFunctionLookup *lookup, int depth,
                              const SlotSummaryOrigin *origin)
{
    unsigned summary = SLOT_PARAM_SUMMARY_NONE;
    unsigned access_mask = 0;
    unsigned escape_mask = 0;

    if (node == NULL || slot_name == NULL)
        return SLOT_PARAM_SUMMARY_NONE;

    access_mask = slot_access_mask_for_named_symbol(
        node, slot_name, lookup, depth);
    escape_mask = slot_escape_mask_in_program(
        node, slot_name, lookup, depth, origin, NULL);

    if ((access_mask & SLOT_ACCESS_READ) != 0)
        summary |= SLOT_PARAM_SUMMARY_READ;
    if ((access_mask & SLOT_ACCESS_WRITE) != 0)
        summary |= SLOT_PARAM_SUMMARY_WRITE;
    if ((access_mask & SLOT_ACCESS_RELEASE) != 0)
        summary |= SLOT_PARAM_SUMMARY_RELEASE;
    if ((escape_mask & SLOT_ESCAPE_RETURN) != 0)
        summary |= SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
    if ((escape_mask & SLOT_ESCAPE_CALL) != 0)
        summary |= SLOT_PARAM_SUMMARY_CALL_ESCAPE;
    if ((escape_mask & SLOT_ESCAPE_CHANNEL) != 0)
        summary |= SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    return summary;
}

unsigned
slot_param_summary_in_program_points(
    ASTNode *const *nodes,
    size_t node_count,
    const char *slot_name,
    const SlotFunctionLookup *lookup,
    int depth,
    const SlotSummaryOrigin *origin)
{
    unsigned summary = SLOT_PARAM_SUMMARY_NONE;

    if (slot_name == NULL || (nodes == NULL && node_count != 0))
        return SLOT_PARAM_SUMMARY_NONE;

    for (size_t i = 0; i < node_count; i++) {
        unsigned access_mask;
        unsigned escape_mask;

        if (nodes[i] == NULL)
            continue;
        access_mask = slot_access_mask_for_named_symbol(
            nodes[i], slot_name, lookup, depth);
        escape_mask = slot_escape_mask_in_program(
            nodes[i], slot_name, lookup, depth, origin, NULL);

        if ((access_mask & SLOT_ACCESS_READ) != 0)
            summary |= SLOT_PARAM_SUMMARY_READ;
        if ((access_mask & SLOT_ACCESS_WRITE) != 0)
            summary |= SLOT_PARAM_SUMMARY_WRITE;
        if ((access_mask & SLOT_ACCESS_RELEASE) != 0)
            summary |= SLOT_PARAM_SUMMARY_RELEASE;
        if ((escape_mask & SLOT_ESCAPE_RETURN) != 0)
            summary |= SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
        if ((escape_mask & SLOT_ESCAPE_CALL) != 0)
            summary |= SLOT_PARAM_SUMMARY_CALL_ESCAPE;
        if ((escape_mask & SLOT_ESCAPE_CHANNEL) != 0)
            summary |= SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    }
    return summary;
}
