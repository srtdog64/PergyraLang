#ifndef PERGYRA_TRANSPILER_MIR_SSA_MAP_H
#define PERGYRA_TRANSPILER_MIR_SSA_MAP_H

/* Small open-addressed SSA name map helpers for MIR C emission. */
static bool
transpiler_parse_versioned_name(const char *versioned, char *base, size_t base_size,
                                size_t *version_out)
{
    const char *dot;
    size_t len;
    if (versioned == NULL || base == NULL || base_size == 0 || version_out == NULL)
        return false;
    dot = strrchr(versioned, '.');
    if (dot == NULL)
        return false;
    len = (size_t)(dot - versioned);
    if (len + 1 > base_size)
        return false;
    memcpy(base, versioned, len);
    base[len] = '\0';
    *version_out = (size_t)strtoull(dot + 1, NULL, 10);
    return true;
}

static size_t
transpiler_ssa_name_bucket_index(const char *key)
{
    size_t hash = 5381u;
    const unsigned char *u = (const unsigned char *)key;
    while (u != NULL && *u != '\0') {
        hash = ((hash << 5) + hash) + (size_t)(*u);
        ++u;
    }
    return hash % TRANSPILE_SSA_NAME_BUCKETS;
}

static void
transpiler_ssa_map_clear(TranspilerSSANameMap *map)
{
    if (map == NULL)
        return;
    for (size_t i = 0; i < TRANSPILE_SSA_NAME_BUCKETS; ++i) {
        if (map->buckets[i].in_use && map->buckets[i].base_name != NULL)
            free((void *)map->buckets[i].base_name);
    }
    memset(map, 0, sizeof(*map));
}

static bool
transpiler_ssa_name_map_set(TranspilerSSANameMap *map,
                            const char *base_name,
                            const char *versioned_name)
{
    size_t idx;
    size_t attempts;

    if (map == NULL || base_name == NULL || versioned_name == NULL)
        return false;
    idx = transpiler_ssa_name_bucket_index(base_name);
    for (attempts = 0; attempts < TRANSPILE_SSA_NAME_BUCKETS; ++attempts) {
        TranspilerSSANameBucket *bucket = &map->buckets[idx];
        if (!bucket->in_use) {
            bucket->in_use = true;
            bucket->base_name = pergyra_strdup(base_name);
            if (bucket->base_name == NULL) {
                bucket->in_use = false;
                return false;
            }
            bucket->versioned_name = versioned_name;
            return true;
        }
        if (bucket->base_name != NULL && strcmp(bucket->base_name, base_name) == 0) {
            bucket->versioned_name = versioned_name;
            return true;
        }
        idx = (idx + 1) % TRANSPILE_SSA_NAME_BUCKETS;
    }
    return false;
}

static bool
transpiler_collect_ssa_name_entries(const char **versioned_values,
                                   size_t value_count,
                                   const char **base_names,
                                   const char **versioned_names,
                                   size_t max_entries,
                                   size_t *map_count_out)
{
    size_t map_count = 0;

    if (versioned_values == NULL
        || base_names == NULL
        || versioned_names == NULL
        || max_entries == 0) {
        return true;
    }
    for (size_t i = 0; i < value_count; i++) {
        const char *versioned = versioned_values[i];
        char base[128];
        size_t parsed_version = 0;
        bool replaced = false;

        if (versioned == NULL)
            continue;
        if (!transpiler_parse_versioned_name(versioned, base, sizeof(base), &parsed_version))
            continue;
        for (size_t j = 0; j < map_count; j++) {
            if (base_names[j] != NULL && strcmp(base_names[j], base) == 0) {
                versioned_names[j] = versioned;
                replaced = true;
                break;
            }
        }
        if (replaced)
            continue;
        if (map_count >= max_entries)
            return false;
        base_names[map_count] = pergyra_strdup(base);
        versioned_names[map_count] = versioned;
        map_count++;
    }
    if (map_count_out != NULL)
        *map_count_out = map_count;
    return true;
}

static void
transpiler_free_ssa_name_entries(const char **base_names, size_t entry_count)
{
    for (size_t i = 0; i < entry_count; i++) {
        free((void *)base_names[i]);
    }
}

static bool
transpiler_rebuild_ssa_map(TranspilerSSANameMap *ssa_map,
                          const char **base_names,
                          const char **versioned_names,
                          size_t map_count)
{
    if (ssa_map == NULL || base_names == NULL || versioned_names == NULL) {
        if (ssa_map != NULL)
            transpiler_ssa_map_clear(ssa_map);
        return false;
    }
    transpiler_ssa_map_clear(ssa_map);
    for (size_t i = 0; i < map_count; i++) {
        if (base_names[i] == NULL || versioned_names[i] == NULL)
            continue;
        if (!transpiler_ssa_name_map_set(ssa_map, base_names[i], versioned_names[i]))
            return false;
    }
    return true;
}

static void
transpiler_emit_mir_block_mapping_comment(CodeBuf *out,
                                          int indent,
                                          const char *routine_name,
                                          const MIRRoutine *routine,
                                          const MIRBasicBlock *block)
{
    uint32_t line = 0;
    uint32_t column = 0;

    if (out == NULL || routine == NULL || block == NULL)
        return;

    if (block->has_source_location) {
        line = block->source_line;
        column = block->source_column;
    }

    if (block->has_source_location) {
        write_indent_to(out, indent);
        codebuf_write(out,
            "/* mir block=%zu hir=%zu (%s) src=%u:%u */\n",
            block->id,
            block->source_hir_block_id,
            routine_name != NULL ? routine_name : "<routine>",
            line,
            column);
    } else {
        write_indent_to(out, indent);
        codebuf_write(out,
            "/* mir block=%zu hir=%s (%s) */\n",
            block->id,
            block->source_hir_block_id == SIZE_MAX ? "<none>" : "mapped",
            routine_name != NULL ? routine_name : "<routine>");
    }
}

#endif /* PERGYRA_TRANSPILER_MIR_SSA_MAP_H */
