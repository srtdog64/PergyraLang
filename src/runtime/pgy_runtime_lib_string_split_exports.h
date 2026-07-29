/* String array construction exports; included after the core string ABI owner. */

/* -----------------------------------------------------------------
 * StringSplit array construction
 * ----------------------------------------------------------------- */
/* StringSplit(str, delim) -> Array<String> (caller-allocated PgyArray_String) */
PgyArray_String StringSplit(const char *s, const char *delim)
{
    PgyArray_String result = pgy_array_new_String(8);
    if (s == NULL || delim == NULL || *delim == '\0') {
        if (s != NULL)
            pgy_array_push_String(&result, pgy_runtime_lib_strdup(s));
        return result;
    }
    size_t dlen = strlen(delim);
    const char *p = s;
    for (;;) {
        const char *found = strstr(p, delim);
        if (found == NULL) {
            pgy_array_push_String(&result, pgy_runtime_lib_strdup(p));
            break;
        }
        size_t seg = (size_t)(found - p);
        char *part = (char *)malloc(seg + 1);
        if (part != NULL) { memcpy(part, p, seg); part[seg] = '\0'; }
        pgy_array_push_String(&result, part != NULL ? part : pgy_runtime_lib_strdup(""));
        p = found + dlen;
    }
    return result;
}
