#include "numeric_parse.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

static bool
pgy_parse_int_internal(const char *text, int *out, bool positive_only,
                       bool strict)
{
    char *end = NULL;
    long parsed;

    if (text == NULL || out == NULL)
        return false;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || parsed < INT_MIN || parsed > INT_MAX)
        return false;
    if (positive_only && parsed <= 0)
        return false;
    if (strict) {
        while (*end != '\0') {
            if (!isspace((unsigned char)*end))
                return false;
            end++;
        }
    }
    *out = (int)parsed;
    return true;
}

bool
pgy_parse_int_prefix(const char *text, int *out)
{
    return pgy_parse_int_internal(text, out, false, false);
}

bool
pgy_parse_positive_int_prefix(const char *text, int *out)
{
    return pgy_parse_int_internal(text, out, true, false);
}

bool
pgy_parse_positive_int_strict(const char *text, int *out)
{
    return pgy_parse_int_internal(text, out, true, true);
}

static bool
pgy_parse_u64_internal(const char *text, uint64_t *out, bool strict,
                       bool allow_zero)
{
    char *end = NULL;
    const char *scan;
    unsigned long long parsed;

    if (text == NULL || out == NULL)
        return false;
    scan = text;
    while (isspace((unsigned char)*scan))
        scan++;
    if (*scan == '-' || *scan == '+')
        return false;
    errno = 0;
    parsed = strtoull(scan, &end, 10);
    if (errno != 0 || end == scan || parsed > (unsigned long long)UINT64_MAX)
        return false;
    if (!allow_zero && parsed == 0)
        return false;
    if (strict) {
        while (*end != '\0') {
            if (!isspace((unsigned char)*end))
                return false;
            end++;
        }
    }
    *out = (uint64_t)parsed;
    return true;
}

static bool
pgy_parse_size_internal(const char *text, size_t *out, bool strict,
                        bool allow_zero)
{
    uint64_t parsed;

    if (out == NULL
        || !pgy_parse_u64_internal(text, &parsed, strict, allow_zero)
        || parsed > (uint64_t)SIZE_MAX) {
        return false;
    }
    *out = (size_t)parsed;
    return true;
}

bool
pgy_parse_size_prefix(const char *text, size_t *out)
{
    return pgy_parse_size_internal(text, out, false, false);
}

bool
pgy_parse_size_strict(const char *text, size_t *out)
{
    return pgy_parse_size_internal(text, out, true, false);
}

bool
pgy_parse_size_strict_allow_zero(const char *text, size_t *out)
{
    return pgy_parse_size_internal(text, out, true, true);
}

bool
pgy_parse_u64_strict_allow_zero(const char *text, uint64_t *out)
{
    return pgy_parse_u64_internal(text, out, true, true);
}
