            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_option_some_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* Result */
        else if (strncmp(pergyra_type_name, "Result<", 7) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_result_ok_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* ZoneChannel<T> */
        else if (strncmp(pergyra_type_name, "ZoneChannel<", 12) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_zone_channel_create_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* WorldChannel<T> */
        else if (strncmp(pergyra_type_name, "WorldChannel<", 13) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_world_channel_create_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* Box<T> */
        else if (strncmp(pergyra_type_name, "Box<", 4) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_box_new_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* Array<T> */
        else if (strncmp(pergyra_type_name, "Array<", 6) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_array_new_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
    }

    /* Fall back to exact match for non-generic types (Future, Qubit, TaskHandle, etc.) */
    t = abi_type_lookup_by_name(pergyra_type_name);
    if (t != NULL)
        return t;

    return NULL;  /* unknown type - backend should handle gracefully */
}

void
mir_abi_table_init(void)
{
    abi_type_table_init();
}
