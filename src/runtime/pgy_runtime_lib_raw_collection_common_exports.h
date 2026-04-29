static char *pgy_runtime_strdup_export(const char *src);

static uint32_t
pgy_hash_string_export(const char *s)
{
    uint32_t h = 2166136261u;
    if (s == NULL)
        return 0;
    while (*s != '\0') {
        h ^= (uint8_t)(*s++);
        h *= 16777619u;
    }
    return h;
}
